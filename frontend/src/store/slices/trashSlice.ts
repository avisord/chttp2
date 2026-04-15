import { createSlice, createAsyncThunk } from '@reduxjs/toolkit'
import * as trashApi from '@/api/trash'
import type { TrashEntry } from '@/api/trash'

interface TrashState {
  items: TrashEntry[]
  status: 'idle' | 'loading' | 'error'
}

const initialState: TrashState = {
  items: [],
  status: 'idle',
}

export const listTrashThunk = createAsyncThunk('trash/list', async () => {
  return trashApi.listTrash()
})

export const restoreItemThunk = createAsyncThunk(
  'trash/restore',
  async (name: string, { dispatch }) => {
    await trashApi.restoreTrashItem(name)
    dispatch(listTrashThunk())
  },
)

export const deleteTrashItemThunk = createAsyncThunk(
  'trash/delete',
  async (name: string, { dispatch }) => {
    await trashApi.deleteTrashItem(name)
    dispatch(listTrashThunk())
  },
)

export const emptyTrashThunk = createAsyncThunk(
  'trash/empty',
  async (_, { dispatch }) => {
    await trashApi.emptyTrash()
    dispatch(listTrashThunk())
  },
)

const trashSlice = createSlice({
  name: 'trash',
  initialState,
  reducers: {},
  extraReducers: (builder) => {
    builder
      .addCase(listTrashThunk.pending, (state) => {
        state.status = 'loading'
      })
      .addCase(listTrashThunk.fulfilled, (state, action) => {
        state.status = 'idle'
        state.items = action.payload
      })
      .addCase(listTrashThunk.rejected, (state) => {
        state.status = 'error'
      })
  },
})

export default trashSlice.reducer
