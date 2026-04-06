import { configureStore } from '@reduxjs/toolkit'
import authReducer from './slices/authSlice'
import fileSystemReducer from './slices/fileSystemSlice'
import sessionsReducer from './slices/sessionsSlice'
import uploadsReducer from './slices/uploadsSlice'
import usersReducer from './slices/usersSlice'
import disksReducer from './slices/disksSlice'
import bookmarksReducer from './slices/bookmarksSlice'
import settingsReducer from './slices/settingsSlice'

export const store = configureStore({
  reducer: {
    auth: authReducer,
    fileSystem: fileSystemReducer,
    sessions: sessionsReducer,
    uploads: uploadsReducer,
    users: usersReducer,
    disks: disksReducer,
    bookmarks: bookmarksReducer,
    settings: settingsReducer,
  },
})

export type RootState = ReturnType<typeof store.getState>
export type AppDispatch = typeof store.dispatch
