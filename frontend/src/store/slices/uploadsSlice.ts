import { createSlice, type PayloadAction } from '@reduxjs/toolkit'
import type { UploadItemState } from '@/lib/uploadEngine'

interface UploadsState {
  items: UploadItemState[]
  visible: boolean
  collapsed: boolean
}

const initialState: UploadsState = {
  items: [],
  visible: false,
  collapsed: false,
}

const uploadsSlice = createSlice({
  name: 'uploads',
  initialState,
  reducers: {
    upsertItem(state, action: PayloadAction<UploadItemState>) {
      const item = action.payload
      // Skip the dummy clear signal
      if (item.id === '__clear__') {
        state.items = state.items.filter(
          (i) => i.phase !== 'complete' && i.phase !== 'error' && i.phase !== 'aborted',
        )
        if (state.items.length === 0) state.visible = false
        return
      }
      const idx = state.items.findIndex((i) => i.id === item.id)
      if (idx >= 0) {
        state.items[idx] = item
      } else {
        state.items.unshift(item)
        state.visible = true
        state.collapsed = false
      }
      // Remove aborted items from the list
      if (item.phase === 'aborted') {
        state.items = state.items.filter((i) => i.id !== item.id)
        if (state.items.length === 0) state.visible = false
      }
    },
    clearDone(state) {
      state.items = state.items.filter(
        (i) => i.phase !== 'complete' && i.phase !== 'error' && i.phase !== 'aborted',
      )
      if (state.items.length === 0) state.visible = false
    },
    removeItem(state, action: PayloadAction<string>) {
      state.items = state.items.filter((i) => i.id !== action.payload)
      if (state.items.length === 0) state.visible = false
    },
    toggleCollapsed(state) {
      state.collapsed = !state.collapsed
    },
    setVisible(state, action: PayloadAction<boolean>) {
      state.visible = action.payload
    },
  },
})

export const { upsertItem, clearDone, removeItem, toggleCollapsed, setVisible } =
  uploadsSlice.actions
export default uploadsSlice.reducer
