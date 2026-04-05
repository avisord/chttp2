import { configureStore } from '@reduxjs/toolkit'
import authReducer from './slices/authSlice'
import fileSystemReducer from './slices/fileSystemSlice'
import sessionsReducer from './slices/sessionsSlice'
import uploadsReducer from './slices/uploadsSlice'

export const store = configureStore({
  reducer: {
    auth: authReducer,
    fileSystem: fileSystemReducer,
    sessions: sessionsReducer,
    uploads: uploadsReducer,
  },
})

export type RootState = ReturnType<typeof store.getState>
export type AppDispatch = typeof store.dispatch
