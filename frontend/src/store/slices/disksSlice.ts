import { createSlice, createAsyncThunk } from '@reduxjs/toolkit'
import * as adminApi from '@/api/admin'
import type { DiskInfo } from '@/types/api'

interface DisksState {
  disks: DiskInfo[]
  status: 'idle' | 'loading' | 'error'
}

const initialState: DisksState = {
  disks: [],
  status: 'idle',
}

export const fetchDisksThunk = createAsyncThunk('disks/fetch', async () => {
  const res = await adminApi.getDisks()
  return res.blockdevices
})

export const mountDiskThunk = createAsyncThunk(
  'disks/mount',
  async (data: { device: string; mountpoint: string; fstype?: string }, { dispatch }) => {
    await adminApi.mountDisk(data)
    dispatch(fetchDisksThunk())
  },
)

export const formatDiskThunk = createAsyncThunk(
  'disks/format',
  async (data: { device: string; fstype: string }, { dispatch }) => {
    await adminApi.formatDisk(data)
    dispatch(fetchDisksThunk())
  },
)

export const unmountDiskThunk = createAsyncThunk(
  'disks/unmount',
  async (data: { mountpoint: string }, { dispatch }) => {
    await adminApi.unmountDisk(data)
    dispatch(fetchDisksThunk())
  },
)

const disksSlice = createSlice({
  name: 'disks',
  initialState,
  reducers: {},
  extraReducers: (builder) => {
    builder
      .addCase(fetchDisksThunk.pending, (state) => {
        state.status = 'loading'
      })
      .addCase(fetchDisksThunk.fulfilled, (state, action) => {
        state.status = 'idle'
        state.disks = action.payload
      })
      .addCase(fetchDisksThunk.rejected, (state) => {
        state.status = 'error'
      })
  },
})

export default disksSlice.reducer
