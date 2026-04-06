import { createSlice, createAsyncThunk, type PayloadAction } from '@reduxjs/toolkit'
import { readContent, writeContent, createDir } from '@/api/filesystem'

const CONFIG_DIR = '.imaginary/config'
const SETTINGS_PATH = '.imaginary/config/settings.json'

interface Settings {
  viewMode: 'grid' | 'list'
  iconSize: number
  [key: string]: unknown
}

const DEFAULTS: Settings = {
  viewMode: 'grid',
  iconSize: 2,
}

interface SettingsState {
  values: Settings
  status: 'idle' | 'loading' | 'loaded' | 'error'
}

const initialState: SettingsState = {
  values: { ...DEFAULTS },
  status: 'idle',
}

let saveTimer: ReturnType<typeof setTimeout> | null = null

function getFullState(thunkApi: { getState: () => unknown }): Settings {
  return (thunkApi.getState() as { settings: SettingsState }).settings.values
}

async function persistSettings(settings: Settings) {
  await writeContent(SETTINGS_PATH, JSON.stringify(settings, null, 2))
}

export const fetchSettingsThunk = createAsyncThunk('settings/fetch', async () => {
  try {
    const res = await readContent(SETTINGS_PATH)
    const parsed = JSON.parse(res.content) as Record<string, unknown>
    return { ...DEFAULTS, ...parsed } as Settings
  } catch {
    try { await createDir(CONFIG_DIR) } catch { /* exists */ }
    await persistSettings(DEFAULTS)
    return DEFAULTS
  }
})

export const saveSettingsThunk = createAsyncThunk(
  'settings/save',
  async (_: void, thunkApi) => {
    const settings = getFullState(thunkApi)
    await persistSettings(settings)
    return settings
  },
)

function debouncedSave(dispatch: (action: unknown) => void) {
  if (saveTimer) clearTimeout(saveTimer)
  saveTimer = setTimeout(() => dispatch(saveSettingsThunk()), 500)
}

const settingsSlice = createSlice({
  name: 'settings',
  initialState,
  reducers: {
    setViewMode(state, action: PayloadAction<'grid' | 'list'>) {
      state.values.viewMode = action.payload
    },
    setIconSize(state, action: PayloadAction<number>) {
      state.values.iconSize = Math.max(1, Math.min(4, action.payload))
    },
  },
  extraReducers: (builder) => {
    builder
      .addCase(fetchSettingsThunk.pending, (state) => {
        state.status = 'loading'
      })
      .addCase(fetchSettingsThunk.fulfilled, (state, action) => {
        state.status = 'loaded'
        state.values = action.payload
      })
      .addCase(fetchSettingsThunk.rejected, (state) => {
        state.status = 'error'
      })
  },
})

export const { setViewMode: setViewModeAction, setIconSize: setIconSizeAction } = settingsSlice.actions

// Thunks that update state AND debounce-save to server
export const setViewMode = (mode: 'grid' | 'list') => (dispatch: (action: unknown) => void) => {
  dispatch(setViewModeAction(mode))
  debouncedSave(dispatch)
}

export const setIconSize = (size: number) => (dispatch: (action: unknown) => void) => {
  dispatch(setIconSizeAction(size))
  debouncedSave(dispatch)
}

export default settingsSlice.reducer
