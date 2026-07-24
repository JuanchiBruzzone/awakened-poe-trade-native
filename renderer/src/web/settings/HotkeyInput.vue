<template>
  <input
    @click="beginNativeCapture"
    @keyup="handleKeyup"
    @keydown.prevent
    :placeholder="capturing ? 'Press A–Z or Backspace' : (modelValue || t('settings.no_key'))"
    :class="{ 'placeholder-red-400': !modelValue }"
    :readonly="noModKeys"
    class="rounded bg-gray-900 px-1 text-center font-poe" />
</template>

<script lang="ts">
import { defineComponent, onUnmounted, PropType, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { KeyToCode, hotkeyToString } from '@ipc/KeyToCode'
import { Host } from '@/web/background/IPC'

export default defineComponent({
  emits: ['update:modelValue'],
  props: {
    modelValue: {
      type: String as PropType<string | null>,
      default: null
    },
    noModKeys: {
      type: Boolean,
      default: false
    },
    required: {
      type: Boolean,
      default: false
    }
  },
  setup (props, ctx) {
    const { t } = useI18n()
    const capturing = ref(false)
    const captureToken = `price-letter-${Math.random().toString(36).slice(2)}`
    const captureEvents = Host.onEvent('MAIN->CLIENT::hotkey-captured', (event) => {
      if (event.token !== captureToken) return
      capturing.value = false
      ctx.emit('update:modelValue', event.key)
    })
    onUnmounted(() => captureEvents.abort())

    return {
      t,
      capturing,
      beginNativeCapture () {
        if (!props.noModKeys) return
        capturing.value = true
        Host.sendEvent({
          name: 'CLIENT->MAIN::begin-hotkey-capture',
          payload: { token: captureToken }
        })
      },
      handleKeyup (e: KeyboardEvent) {
        e.preventDefault()
        e.stopPropagation()

        if (e.code === 'Backspace') {
          if (!props.required) {
            ctx.emit('update:modelValue', null)
          }
          return
        }

        let { code, ctrlKey, shiftKey, altKey } = e

        if (code.startsWith('Key')) {
          code = code.slice('Key'.length)
        } else if (code.startsWith('Digit')) {
          code = code.slice('Digit'.length)
        } else if (e.key === 'Cancel' && code === 'Pause') {
          code = 'Cancel'
        }

        if ((KeyToCode as Record<string, number>)[code]) {
          code = hotkeyToString([code], ctrlKey, shiftKey, altKey)
          if (code.includes('F12')) return
          if (props.noModKeys && code.includes('+')) return
          ctx.emit('update:modelValue', code)
        }
      }
    }
  }
})
</script>
