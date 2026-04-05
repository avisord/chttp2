import { createSlice, createAsyncThunk } from '@reduxjs/toolkit'
import * as authApi from '@/api/auth'
import type { WhoamiResponse } from '@/types/api'

interface AuthState {
  user: WhoamiResponse | null
  status: 'idle' | 'loading' | 'authenticated' | 'unauthenticated'
  loginError: string | null
}

const initialState: AuthState = {
  user: null,
  status: 'idle',
  loginError: null,
}

export const loginThunk = createAsyncThunk(
  'auth/login',
  async ({ username, password }: { username: string; password: string }, { dispatch }) => {
    await authApi.login(username, password)
    const user = await authApi.whoami()
    dispatch(authSlice.actions.setUser(user))
    return user
  },
)

export const whoamiThunk = createAsyncThunk('auth/whoami', async () => {
  return authApi.whoami()
})

export const logoutThunk = createAsyncThunk('auth/logout', async () => {
  await authApi.logout()
})

const authSlice = createSlice({
  name: 'auth',
  initialState,
  reducers: {
    setUser(state, action) {
      state.user = action.payload
      state.status = 'authenticated'
    },
    clearAuth(state) {
      state.user = null
      state.status = 'unauthenticated'
    },
  },
  extraReducers: (builder) => {
    builder
      .addCase(loginThunk.pending, (state) => {
        state.status = 'loading'
        state.loginError = null
      })
      .addCase(loginThunk.fulfilled, (state, action) => {
        state.user = action.payload
        state.status = 'authenticated'
        state.loginError = null
      })
      .addCase(loginThunk.rejected, (state, action) => {
        state.status = 'unauthenticated'
        state.loginError = action.error.message || 'Login failed'
      })
      .addCase(whoamiThunk.pending, (state) => {
        if (state.status === 'idle') state.status = 'loading'
      })
      .addCase(whoamiThunk.fulfilled, (state, action) => {
        state.user = action.payload
        state.status = 'authenticated'
      })
      .addCase(whoamiThunk.rejected, (state) => {
        state.user = null
        state.status = 'unauthenticated'
      })
      .addCase(logoutThunk.fulfilled, (state) => {
        state.user = null
        state.status = 'unauthenticated'
      })
  },
})

export const { setUser, clearAuth } = authSlice.actions
export default authSlice.reducer
