#include "HeistOcrService.h"

#include "Logger.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>

#ifdef APT_HAS_NATIVE_OCR
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <tesseract/baseapi.h>
#endif

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace AptNative {

struct HeistOcrService::State {
    QString binDirectory;
    QString language = QStringLiteral("en");
    QMutex mutex;
};

#ifdef APT_HAS_NATIVE_OCR
namespace {

constexpr int ReferenceHeight = 600;
constexpr float TemplateThreshold = 0.75F;
constexpr int LineYTolerance = 4;

struct WeightedPoint {
    int x;
    int y;
    float weight;
};

std::vector<WeightedPoint> findNonZeroWeights(const cv::Mat &matches)
{
    cv::Mat locations;
    cv::findNonZero(matches, locations);
    std::vector<WeightedPoint> weights;
    weights.reserve(static_cast<size_t>(locations.rows));
    for (int row = 0; row < locations.rows; ++row) {
        const cv::Point point = locations.at<cv::Point>(row);
        weights.push_back({point.x, point.y, matches.at<float>(point.y, point.x)});
    }
    return weights;
}

std::vector<WeightedPoint> groupWeightedPoints(
    const std::vector<WeightedPoint> &weights,
    double radius)
{
    std::vector<WeightedPoint> maxima;
    for (const WeightedPoint &point : weights) {
        const auto close = std::find_if(maxima.begin(), maxima.end(),
            [&point, radius](const WeightedPoint &candidate) {
                return std::hypot(
                    static_cast<double>(point.x - candidate.x),
                    static_cast<double>(point.y - candidate.y)) < radius;
            });
        if (close == maxima.end()) {
            maxima.push_back(point);
        } else if (point.weight > close->weight) {
            *close = point;
        }
    }
    return maxima;
}

using Line = std::pair<WeightedPoint, WeightedPoint>;

std::vector<Line> findLines(std::vector<WeightedPoint> points)
{
    std::sort(points.begin(), points.end(),
              [](const WeightedPoint &left, const WeightedPoint &right) {
        return left.x < right.x;
    });
    std::vector<Line> lines;
    for (size_t first = 0; first < points.size(); ++first) {
        for (size_t second = first + 1; second < points.size(); ++second) {
            if (std::abs(points[first].y - points[second].y) > LineYTolerance) continue;
            lines.emplace_back(points[first], points[second]);
            break;
        }
    }
    return lines;
}

QString tesseractLanguage(const QString &language)
{
    if (language == QStringLiteral("ru")) return QStringLiteral("rus");
    if (language == QStringLiteral("en")) return QStringLiteral("eng");
    return {};
}

HeistOcrResult recognizeScreenshot(
    const QString &binDirectory,
    const QString &configuredLanguage,
    const QImage &source)
{
    QElapsedTimer timer;
    timer.start();

    const QString language = tesseractLanguage(configuredLanguage);
    if (language.isEmpty()) {
        return {0, {}, QStringLiteral("OCR does not support language \"%1\".")
            .arg(configuredLanguage)};
    }

    const QString templatePath = binDirectory + QStringLiteral("/heist-lock.bmp");
    const cv::Mat needle = cv::imread(templatePath.toStdString(), cv::IMREAD_GRAYSCALE);
    if (needle.empty()) {
        return {0, {}, QStringLiteral(
            "Missing apt-data/cv-ocr/heist-lock.bmp. Install the OCR data pack from the OCR Guide.")};
    }
    if (source.isNull() || source.height() <= 0) {
        return {0, {}, QStringLiteral("The game screenshot is empty.")};
    }

    const QImage bgra = source.convertToFormat(QImage::Format_ARGB32);
    cv::Mat color(
        bgra.height(),
        bgra.width(),
        CV_8UC4,
        const_cast<uchar *>(bgra.constBits()),
        static_cast<size_t>(bgra.bytesPerLine()));

    const double scale = static_cast<double>(bgra.height()) / ReferenceHeight;
    const int normalizedWidth = static_cast<int>(
        std::floor(static_cast<double>(bgra.width()) / scale));
    if (normalizedWidth < needle.cols || ReferenceHeight < needle.rows) {
        return {0, {}, QStringLiteral("The game screenshot is too small for Heist OCR.")};
    }

    cv::Mat resized;
    if (scale > 2.1) {
        cv::resize(color, resized,
                   cv::Size(normalizedWidth * 2, ReferenceHeight * 2),
                   0, 0, cv::INTER_LINEAR);
        cv::resize(resized, resized,
                   cv::Size(normalizedWidth, ReferenceHeight),
                   0, 0, cv::INTER_LINEAR);
    } else {
        cv::resize(color, resized,
                   cv::Size(normalizedWidth, ReferenceHeight),
                   0, 0, cv::INTER_LINEAR);
    }
    cv::Mat gray;
    cv::cvtColor(resized, gray, cv::COLOR_BGRA2GRAY);

    cv::Mat matches;
    cv::matchTemplate(gray, needle, matches, cv::TM_CCOEFF_NORMED);
    cv::threshold(matches, matches, TemplateThreshold, 1, cv::THRESH_TOZERO);
    const std::vector<WeightedPoint> weighted = findNonZeroWeights(matches);
    const std::vector<WeightedPoint> clustered =
        groupWeightedPoints(weighted, std::hypot(needle.cols, needle.rows));
    const std::vector<Line> lines = findLines(clustered);

    tesseract::TessBaseAPI tess;
    const QByteArray langBytes = language.toUtf8();
    const QString customData = binDirectory + QLatin1Char('/') + language +
                               QStringLiteral(".traineddata");
    const QByteArray dataBytes = binDirectory.toUtf8();
    const char *dataPath = QFileInfo::exists(customData) ? dataBytes.constData() : nullptr;
    if (tess.Init(dataPath, langBytes.constData()) != 0) {
        return {0, {}, QStringLiteral("Tesseract could not initialize language \"%1\".")
            .arg(language)};
    }
    tess.SetPageSegMode(tesseract::PSM_SINGLE_LINE);

    QStringList paragraphs;
    for (const Line &line : lines) {
        const int left = static_cast<int>(std::floor(
            (std::min(line.first.x, line.second.x) + needle.cols) * scale));
        const int top = static_cast<int>(std::floor(
            (std::min(line.first.y, line.second.y) - 1) * scale));
        const int right = static_cast<int>(std::floor(
            std::max(line.first.x, line.second.x) * scale));
        const int bottom = static_cast<int>(std::floor(
            (std::max(line.first.y, line.second.y) + needle.rows) * scale));
        const cv::Rect bounds(0, 0, color.cols, color.rows);
        const cv::Rect roi = cv::Rect(left, top, right - left, bottom - top) & bounds;
        if (roi.width <= 0 || roi.height <= 0) continue;

        cv::Mat hsv;
        cv::cvtColor(color(roi), hsv, cv::COLOR_BGRA2BGR);
        cv::cvtColor(hsv, hsv, cv::COLOR_BGR2HSV_FULL);
        cv::inRange(
            hsv,
            cv::Scalar(123, 79, 79),
            cv::Scalar(128, 255, 255),
            hsv);
        cv::bitwise_not(hsv, hsv);

        tess.SetImage(hsv.data, hsv.cols, hsv.rows, 1,
                      static_cast<int>(hsv.step));
        if (tess.Recognize(nullptr) != 0) continue;
        std::unique_ptr<char[]> rawText(tess.GetUTF8Text());
        const QString text = rawText
            ? QString::fromUtf8(rawText.get()).trimmed()
            : QString{};
        if (!text.isEmpty() && tess.MeanTextConf() > 30) {
            paragraphs.append(text);
        }
        tess.Clear();
    }
    tess.End();
    return {timer.elapsed(), paragraphs, {}};
}

} // namespace
#endif

HeistOcrService::HeistOcrService(
    QString dataDirectory,
    Logger *logger,
    QObject *parent)
    : QObject(parent),
      m_state(std::make_unique<State>()),
      m_logger(logger)
{
    m_state->binDirectory = std::move(dataDirectory) +
                            QStringLiteral("/cv-ocr");
}

HeistOcrService::~HeistOcrService() = default;

void HeistOcrService::setLanguage(const QString &language)
{
    QMutexLocker lock(&m_state->mutex);
    m_state->language = language;
}

void HeistOcrService::recognize(
    const QImage &screenshot,
    std::function<void(HeistOcrResult)> onFinished)
{
    if (m_pending) {
        onFinished({0, {}, QStringLiteral("Heist OCR is already running.")});
        return;
    }
#ifndef APT_HAS_NATIVE_OCR
    Q_UNUSED(screenshot);
    onFinished({0, {}, QStringLiteral(
        "This build does not include native OpenCV and Tesseract support.")});
#else
    m_pending = true;
    QString binDirectory;
    QString language;
    {
        QMutexLocker lock(&m_state->mutex);
        binDirectory = m_state->binDirectory;
        language = m_state->language;
    }
    auto *watcher = new QFutureWatcher<HeistOcrResult>(this);
    connect(watcher, &QFutureWatcher<HeistOcrResult>::finished, this,
            [this, watcher, onFinished = std::move(onFinished)]() mutable {
        const HeistOcrResult result = watcher->result();
        watcher->deleteLater();
        m_pending = false;
        if (!result.error.isEmpty()) {
            m_logger->write(QStringLiteral("error [OCR] %1").arg(result.error));
        }
        onFinished(result);
    });
    watcher->setFuture(QtConcurrent::run(
        recognizeScreenshot, binDirectory, language, screenshot));
#endif
}

} // namespace AptNative
