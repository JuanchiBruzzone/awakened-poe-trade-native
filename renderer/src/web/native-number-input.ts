import { Host } from '@/web/background/IPC'

function updateInput (input: HTMLInputElement, value: string) {
  input.value = value
  input.dispatchEvent(new Event('input', { bubbles: true }))
}

export function installNativeNumberInputCapture () {
  const token = `number-input-${Math.random().toString(36).slice(2)}`
  let input: HTMLInputElement | null = null
  let original = ''
  let buffer = ''
  let replace = true

  const finish = () => {
    input = null
    replace = true
  }

  const captured = Host.onEvent('MAIN->CLIENT::number-captured', (event) => {
    if (event.token !== token || input == null) return

    if (event.key === 'Escape') {
      updateInput(input, original)
      finish()
      return
    }
    if (event.key === 'Enter') {
      input.dispatchEvent(new Event('change', { bubbles: true }))
      finish()
      return
    }
    if (event.key === 'Delete') {
      buffer = ''
    } else if (event.key === 'Backspace') {
      buffer = replace ? '' : buffer.slice(0, -1)
    } else if (event.key === '.') {
      if (replace) buffer = '0.'
      else if (!buffer.includes('.')) buffer += '.'
    } else if (event.key === '-') {
      buffer = buffer.startsWith('-') ? buffer.slice(1) : `-${replace ? '' : buffer}`
    } else if (/^[0-9]$/.test(event.key)) {
      buffer = replace ? event.key : `${buffer}${event.key}`
    }
    replace = false
    updateInput(input, buffer)
  })

  document.addEventListener('pointerdown', (event) => {
    const target = event.target
    const numberInput = target instanceof HTMLInputElement &&
      target.type === 'number'
      ? target
      : null

    if (numberInput == null) {
      if (input != null) {
        Host.sendEvent({
          name: 'CLIENT->MAIN::cancel-number-capture',
          payload: { token }
        })
        finish()
      }
      return
    }

    input = numberInput
    original = numberInput.value
    buffer = original
    replace = true
    Host.sendEvent({
      name: 'CLIENT->MAIN::begin-number-capture',
      payload: { token }
    })
  }, true)

  window.addEventListener('beforeunload', () => {
    captured.abort()
  })
}
