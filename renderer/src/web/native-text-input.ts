import { Host } from '@/web/background/IPC'

type TextControl = HTMLInputElement | HTMLTextAreaElement

function isTextControl (target: EventTarget | null): target is TextControl {
  if (target instanceof HTMLTextAreaElement) return !target.readOnly && !target.disabled
  if (!(target instanceof HTMLInputElement) || target.readOnly || target.disabled) return false
  return ['text', 'search', 'url', 'email', 'tel', 'password'].includes(target.type)
}

function emitInput (input: TextControl) {
  input.dispatchEvent(new Event('input', { bubbles: true }))
}

function replaceSelection (input: TextControl, text: string) {
  const start = input.selectionStart ?? input.value.length
  const end = input.selectionEnd ?? start
  input.setRangeText(text, start, end, 'end')
  emitInput(input)
}

function moveCursor (input: TextControl, direction: -1 | 1, selecting: boolean) {
  const start = input.selectionStart ?? 0
  const end = input.selectionEnd ?? start
  const anchor = selecting ? start : (direction < 0 ? start : end)
  const position = Math.max(0, Math.min(input.value.length,
    (direction < 0 ? start : end) + direction))
  input.setSelectionRange(selecting ? anchor : position, position)
}

export function installNativeTextInputCapture () {
  const token = `text-input-${Math.random().toString(36).slice(2)}`
  let input: TextControl | null = null

  const cancel = () => {
    if (input == null) return
    Host.sendEvent({
      name: 'CLIENT->MAIN::cancel-text-capture',
      payload: { token }
    })
    input = null
  }

  Host.onEvent('MAIN->CLIENT::text-captured', (event) => {
    if (event.token !== token || input == null) return

    const { key } = event
    if (key === 'Backspace' || key === 'Delete') {
      const start = input.selectionStart ?? 0
      const end = input.selectionEnd ?? start
      if (start !== end) {
        input.setRangeText('', start, end, 'end')
      } else if (key === 'Backspace' && start > 0) {
        input.setRangeText('', start - 1, start, 'end')
      } else if (key === 'Delete' && end < input.value.length) {
        input.setRangeText('', end, end + 1, 'end')
      }
      emitInput(input)
    } else if (key === 'ArrowLeft' || key === 'ArrowRight') {
      moveCursor(input, key === 'ArrowLeft' ? -1 : 1, false)
    } else if (key === 'Shift+ArrowLeft' || key === 'Shift+ArrowRight') {
      moveCursor(input, key === 'Shift+ArrowLeft' ? -1 : 1, true)
    } else if (key === 'Home' || key === 'End') {
      const position = key === 'Home' ? 0 : input.value.length
      input.setSelectionRange(position, position)
    } else if (key === 'SelectAll') {
      input.select()
    } else if (key === 'Copy' || key === 'Cut') {
      const start = input.selectionStart ?? 0
      const end = input.selectionEnd ?? start
      if (start !== end) {
        Host.sendEvent({
          name: 'CLIENT->MAIN::set-clipboard-text',
          payload: { text: input.value.slice(start, end) }
        })
        if (key === 'Cut') {
          input.setRangeText('', start, end, 'end')
          emitInput(input)
        }
      }
    } else if (key === 'Paste') {
      replaceSelection(input, event.text ?? '')
    } else if (key === 'Enter') {
      if (input instanceof HTMLTextAreaElement) {
        replaceSelection(input, '\n')
      } else {
        input.dispatchEvent(new Event('change', { bubbles: true }))
        input.blur()
        cancel()
      }
    } else if (key === 'Tab') {
      const controls = Array.from(document.querySelectorAll<TextControl>(
        'input:not([type=number]):not([type=file]):not([readonly]):not([disabled]), textarea:not([readonly]):not([disabled])'
      )).filter(element => element.offsetParent != null)
      const next = controls.length === 0
        ? undefined
        : controls[(controls.indexOf(input) + 1) % controls.length]
      if (next != null) {
        input = next
        next.focus()
        next.select()
      }
    } else {
      replaceSelection(input, key)
    }
  })

  document.addEventListener('pointerdown', (event) => {
    if (!isTextControl(event.target)) {
      cancel()
      return
    }
    input = event.target
    Host.sendEvent({
      name: 'CLIENT->MAIN::begin-text-capture',
      payload: { token }
    })
  }, true)

  window.addEventListener('beforeunload', cancel)
}
