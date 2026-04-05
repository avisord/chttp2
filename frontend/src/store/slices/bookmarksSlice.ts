import { createSlice, createAsyncThunk, type PayloadAction } from '@reduxjs/toolkit'
import { readContent, writeContent, createDir } from '@/api/filesystem'

export interface Bookmark {
  path: string
  label: string
}

const BOOKMARKS_PATH = '.imaginary/places'
const IMAGINARY_DIR = '.imaginary'

const DEFAULT_BOOKMARKS: Bookmark[] = [
  { path: '/', label: 'Root' },
  { path: '.', label: 'Home' },
]

function parseBookmarks(text: string): Bookmark[] {
  return text
    .split('\n')
    .map((line) => line.trim())
    .filter((line) => line && !line.startsWith('#'))
    .map((line) => {
      const tab = line.indexOf('\t')
      if (tab === -1) return { path: line, label: line }
      return { path: line.slice(0, tab), label: line.slice(tab + 1) }
    })
}

function serializeBookmarks(bookmarks: Bookmark[]): string {
  return bookmarks.map((b) => `${b.path}\t${b.label}`).join('\n') + '\n'
}

interface BookmarksState {
  bookmarks: Bookmark[]
  status: 'idle' | 'loading' | 'error'
}

const initialState: BookmarksState = {
  bookmarks: DEFAULT_BOOKMARKS,
  status: 'idle',
}

export const fetchBookmarksThunk = createAsyncThunk('bookmarks/fetch', async () => {
  try {
    const res = await readContent(BOOKMARKS_PATH)
    const parsed = parseBookmarks(res.content)
    return parsed.length ? parsed : DEFAULT_BOOKMARKS
  } catch {
    // File doesn't exist — create .imaginary dir and default file
    try {
      await createDir(IMAGINARY_DIR)
    } catch { /* dir may already exist */ }
    await writeContent(BOOKMARKS_PATH, serializeBookmarks(DEFAULT_BOOKMARKS))
    return DEFAULT_BOOKMARKS
  }
})

export const addBookmarkThunk = createAsyncThunk(
  'bookmarks/add',
  async (bookmark: Bookmark, { getState }) => {
    const state = getState() as { bookmarks: BookmarksState }
    const existing = state.bookmarks.bookmarks
    if (existing.some((b) => b.path === bookmark.path)) return existing
    const updated = [...existing, bookmark]
    await writeContent(BOOKMARKS_PATH, serializeBookmarks(updated))
    return updated
  },
)

export const removeBookmarkThunk = createAsyncThunk(
  'bookmarks/remove',
  async (path: string, { getState }) => {
    const state = getState() as { bookmarks: BookmarksState }
    const updated = state.bookmarks.bookmarks.filter((b) => b.path !== path)
    await writeContent(BOOKMARKS_PATH, serializeBookmarks(updated))
    return updated
  },
)

export const reorderBookmarksThunk = createAsyncThunk(
  'bookmarks/reorder',
  async (bookmarks: Bookmark[]) => {
    await writeContent(BOOKMARKS_PATH, serializeBookmarks(bookmarks))
    return bookmarks
  },
)

const bookmarksSlice = createSlice({
  name: 'bookmarks',
  initialState,
  reducers: {
    setBookmarks(state, action: PayloadAction<Bookmark[]>) {
      state.bookmarks = action.payload
    },
  },
  extraReducers: (builder) => {
    builder
      .addCase(fetchBookmarksThunk.pending, (state) => {
        state.status = 'loading'
      })
      .addCase(fetchBookmarksThunk.fulfilled, (state, action) => {
        state.status = 'idle'
        state.bookmarks = action.payload
      })
      .addCase(fetchBookmarksThunk.rejected, (state) => {
        state.status = 'error'
      })
      .addCase(addBookmarkThunk.fulfilled, (state, action) => {
        state.bookmarks = action.payload
      })
      .addCase(removeBookmarkThunk.fulfilled, (state, action) => {
        state.bookmarks = action.payload
      })
      .addCase(reorderBookmarksThunk.fulfilled, (state, action) => {
        state.bookmarks = action.payload
      })
  },
})

export const { setBookmarks } = bookmarksSlice.actions
export default bookmarksSlice.reducer
