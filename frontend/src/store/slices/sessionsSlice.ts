import { createSlice, createAsyncThunk } from '@reduxjs/toolkit'
import * as sessionsApi from '@/api/sessions'
import { whoamiThunk } from './authSlice'
import type { SessionInfo } from '@/types/api'

interface SessionsState {
  sessions: SessionInfo[]
  status: 'idle' | 'loading' | 'error'
}

const initialState: SessionsState = {
  sessions: [],
  status: 'idle',
}

export const fetchSessionsThunk = createAsyncThunk('sessions/fetch', async () => {
  return sessionsApi.getSessions()
})

export const deleteSessionThunk = createAsyncThunk(
  'sessions/delete',
  async (sessionId: string, { dispatch }) => {
    await sessionsApi.deleteSession(sessionId)
    dispatch(fetchSessionsThunk())
  },
)

export const switchSessionThunk = createAsyncThunk(
  'sessions/switch',
  async (sessionId: string, { dispatch }) => {
    await sessionsApi.switchSession(sessionId)
    await dispatch(whoamiThunk()).unwrap()
  },
)

const sessionsSlice = createSlice({
  name: 'sessions',
  initialState,
  reducers: {},
  extraReducers: (builder) => {
    builder
      .addCase(fetchSessionsThunk.pending, (state) => {
        state.status = 'loading'
      })
      .addCase(fetchSessionsThunk.fulfilled, (state, action) => {
        state.status = 'idle'
        state.sessions = action.payload
      })
      .addCase(fetchSessionsThunk.rejected, (state) => {
        state.status = 'error'
      })
  },
})

export default sessionsSlice.reducer
