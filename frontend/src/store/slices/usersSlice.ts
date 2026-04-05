import { createSlice, createAsyncThunk } from '@reduxjs/toolkit'
import * as adminApi from '@/api/admin'
import type { SystemUser } from '@/types/api'

interface UsersState {
  users: SystemUser[]
  status: 'idle' | 'loading' | 'error'
}

const initialState: UsersState = {
  users: [],
  status: 'idle',
}

export const fetchUsersThunk = createAsyncThunk('users/fetch', async () => {
  return adminApi.getUsers()
})

export const createUserThunk = createAsyncThunk(
  'users/create',
  async (data: { username: string; password: string; shell?: string }, { dispatch }) => {
    await adminApi.createUser(data)
    dispatch(fetchUsersThunk())
  },
)

export const editUserThunk = createAsyncThunk(
  'users/edit',
  async ({ username, data }: { username: string; data: { password?: string; shell?: string; groups?: string } }, { dispatch }) => {
    await adminApi.editUser(username, data)
    dispatch(fetchUsersThunk())
  },
)

export const deleteUserThunk = createAsyncThunk(
  'users/delete',
  async (username: string, { dispatch }) => {
    await adminApi.deleteUser(username)
    dispatch(fetchUsersThunk())
  },
)

const usersSlice = createSlice({
  name: 'users',
  initialState,
  reducers: {},
  extraReducers: (builder) => {
    builder
      .addCase(fetchUsersThunk.pending, (state) => {
        state.status = 'loading'
      })
      .addCase(fetchUsersThunk.fulfilled, (state, action) => {
        state.status = 'idle'
        state.users = action.payload
      })
      .addCase(fetchUsersThunk.rejected, (state) => {
        state.status = 'error'
      })
  },
})

export default usersSlice.reducer
