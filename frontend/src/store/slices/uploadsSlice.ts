import { createSlice, type PayloadAction } from '@reduxjs/toolkit'

export interface UploadItem {
  id: string
  filename: string
  totalSize: number
  uploadedSize: number
  status: 'pending' | 'uploading' | 'completed' | 'error'
  error?: string
}

interface UploadsState {
  items: UploadItem[]
  visible: boolean
}

const initialState: UploadsState = {
  items: [],
  visible: false,
}

const uploadsSlice = createSlice({
  name: 'uploads',
  initialState,
  reducers: {
    addUpload(state, action: PayloadAction<UploadItem>) {
      state.items.unshift(action.payload)
      state.visible = true
    },
    updateProgress(state, action: PayloadAction<{ id: string; uploadedSize: number }>) {
      const item = state.items.find((i) => i.id === action.payload.id)
      if (item) {
        item.uploadedSize = action.payload.uploadedSize
        item.status = 'uploading'
      }
    },
    setUploadStatus(
      state,
      action: PayloadAction<{ id: string; status: UploadItem['status']; error?: string }>,
    ) {
      const item = state.items.find((i) => i.id === action.payload.id)
      if (item) {
        item.status = action.payload.status
        if (action.payload.error) item.error = action.payload.error
        if (action.payload.status === 'completed') item.uploadedSize = item.totalSize
      }
    },
    removeUpload(state, action: PayloadAction<string>) {
      state.items = state.items.filter((i) => i.id !== action.payload)
      if (state.items.length === 0) state.visible = false
    },
    clearCompleted(state) {
      state.items = state.items.filter((i) => i.status !== 'completed')
      if (state.items.length === 0) state.visible = false
    },
    toggleVisible(state) {
      state.visible = !state.visible
    },
  },
})

export const { addUpload, updateProgress, setUploadStatus, removeUpload, clearCompleted, toggleVisible } =
  uploadsSlice.actions
export default uploadsSlice.reducer
