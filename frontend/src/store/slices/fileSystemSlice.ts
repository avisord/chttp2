import { createSlice, createAsyncThunk, type PayloadAction } from '@reduxjs/toolkit'
import * as fsApi from '@/api/filesystem'
import type { FileEntry, FileStat } from '@/types/api'

interface FileSystemState {
  currentPath: string
  entries: FileEntry[]
  status: 'idle' | 'loading' | 'error'
  error: string | null
  sortBy: 'name' | 'size' | 'modified'
  sortOrder: 'asc' | 'desc'
  selectedPaths: string[]
  previewFile: FileEntry | null
  previewContent: string | null
  fileStat: FileStat | null
  clipboard: { operation: 'copy' | 'cut'; paths: string[] } | null
}

const initialState: FileSystemState = {
  currentPath: '.',
  entries: [],
  status: 'idle',
  error: null,
  sortBy: 'name',
  sortOrder: 'asc',
  selectedPaths: [],
  previewFile: null,
  previewContent: null,
  fileStat: null,
  clipboard: null,
}

export const listDirThunk = createAsyncThunk('fs/listDir', async (path: string) => {
  return fsApi.listDir(path)
})

export const createDirThunk = createAsyncThunk(
  'fs/createDir',
  async (path: string, { dispatch, getState }) => {
    await fsApi.createDir(path)
    const state = getState() as { fileSystem: FileSystemState }
    dispatch(listDirThunk(state.fileSystem.currentPath))
  },
)

export const deleteFileThunk = createAsyncThunk(
  'fs/deleteFile',
  async (path: string, { dispatch, getState }) => {
    await fsApi.deleteFile(path)
    const state = getState() as { fileSystem: FileSystemState }
    dispatch(listDirThunk(state.fileSystem.currentPath))
  },
)

export const deleteDirThunk = createAsyncThunk(
  'fs/deleteDir',
  async (path: string, { dispatch, getState }) => {
    await fsApi.deleteDir(path)
    const state = getState() as { fileSystem: FileSystemState }
    dispatch(listDirThunk(state.fileSystem.currentPath))
  },
)

export const renameFileThunk = createAsyncThunk(
  'fs/renameFile',
  async ({ path, name }: { path: string; name: string }, { dispatch, getState }) => {
    await fsApi.renameFile(path, name)
    const state = getState() as { fileSystem: FileSystemState }
    dispatch(listDirThunk(state.fileSystem.currentPath))
  },
)

export const moveFileThunk = createAsyncThunk(
  'fs/moveFile',
  async ({ from, to }: { from: string; to: string }, { dispatch, getState }) => {
    await fsApi.moveFile(from, to)
    const state = getState() as { fileSystem: FileSystemState }
    dispatch(listDirThunk(state.fileSystem.currentPath))
  },
)

export const copyFileThunk = createAsyncThunk(
  'fs/copyFile',
  async ({ from, to }: { from: string; to: string }, { dispatch, getState }) => {
    await fsApi.copyFile(from, to)
    const state = getState() as { fileSystem: FileSystemState }
    dispatch(listDirThunk(state.fileSystem.currentPath))
  },
)

export const fetchStatThunk = createAsyncThunk('fs/fetchStat', async (path: string) => {
  return fsApi.fetchStat(path)
})

export const fetchContentThunk = createAsyncThunk('fs/fetchContent', async (path: string) => {
  const res = await fsApi.readContent(path)
  return res.content
})

const fileSystemSlice = createSlice({
  name: 'fileSystem',
  initialState,
  reducers: {
    setCurrentPath(state, action: PayloadAction<string>) {
      state.currentPath = action.payload
      state.selectedPaths = []
      state.previewFile = null
      state.previewContent = null
      state.fileStat = null
    },
    setSortBy(state, action: PayloadAction<'name' | 'size' | 'modified'>) {
      if (state.sortBy === action.payload) {
        state.sortOrder = state.sortOrder === 'asc' ? 'desc' : 'asc'
      } else {
        state.sortBy = action.payload
        state.sortOrder = 'asc'
      }
    },
    setSelectedPaths(state, action: PayloadAction<string[]>) {
      state.selectedPaths = action.payload
    },
    toggleSelect(state, action: PayloadAction<string>) {
      const idx = state.selectedPaths.indexOf(action.payload)
      if (idx >= 0) state.selectedPaths.splice(idx, 1)
      else state.selectedPaths.push(action.payload)
    },
    selectAll(state) {
      state.selectedPaths = state.entries.map((e) => e.name)
    },
    clearSelection(state) {
      state.selectedPaths = []
    },
    setPreviewFile(state, action: PayloadAction<FileEntry | null>) {
      state.previewFile = action.payload
      if (!action.payload) {
        state.previewContent = null
        state.fileStat = null
      }
    },
    setClipboard(state, action: PayloadAction<{ operation: 'copy' | 'cut'; paths: string[] } | null>) {
      state.clipboard = action.payload
    },
  },
  extraReducers: (builder) => {
    builder
      .addCase(listDirThunk.pending, (state) => {
        state.status = 'loading'
        state.error = null
      })
      .addCase(listDirThunk.fulfilled, (state, action) => {
        state.status = 'idle'
        state.entries = action.payload.entries
        state.currentPath = action.payload.path
      })
      .addCase(listDirThunk.rejected, (state, action) => {
        state.status = 'error'
        state.error = action.error.message || 'Failed to list directory'
      })
      .addCase(fetchStatThunk.fulfilled, (state, action) => {
        state.fileStat = action.payload
      })
      .addCase(fetchContentThunk.fulfilled, (state, action) => {
        state.previewContent = action.payload
      })
  },
})

export const {
  setCurrentPath,
  setSortBy,
  setSelectedPaths,
  toggleSelect,
  selectAll,
  clearSelection,
  setPreviewFile,
  setClipboard,
} = fileSystemSlice.actions
export default fileSystemSlice.reducer
