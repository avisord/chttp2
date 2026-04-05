# Frontend Dashboard for chttp2 NAS Server

## Context

The chttp2 project is a C-based HTTP server that provides a NAS/file management API with PAM authentication, session management, and full filesystem CRUD. The existing frontend is minimal vanilla HTML/JS (login + basic whoami dashboard). The goal is to build a **modern React SPA** that serves as a complete dashboard for all 22+ API endpoints, providing a polished file browser experience with uploads, previews, and session management.

The existing frontend uses a dark theme with indigo accents, radial gradient backgrounds, and glassmorphism cards — the new React app will carry this DNA forward using the "Corporate Trust" design system.

---

## Stack

- **React 19** (Vite) + **TypeScript**
- **TailwindCSS v4** (with `@tailwindcss/vite` plugin)
- **shadcn/ui** (customized to Corporate Trust design tokens)
- **Redux Toolkit** (slices + async thunks)
- **react-router-dom v7** (client-side routing)
- **lucide-react** (icons)

---

## API Summary

All endpoints on the C backend (port 8080). Auth is cookie-based (`HttpOnly` cookies set by `/login`).

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/login` | `{username, password}` → sets session cookies |
| GET | `/whoami` | Returns authenticated user info |
| DELETE | `/logout` | Clears session, returns to login |
| GET | `/sessions` | List all sessions from cookies |
| DELETE | `/sessions/:id` | Delete a specific session |
| POST | `/sessions/switch/:id` | Switch active session |
| GET | `/fs/list?path=` | List directory entries |
| POST | `/fs/upload?path=` | Multipart file upload |
| GET | `/fs/download?path=` | Stream file download |
| DELETE | `/fs/file?path=` | Delete file |
| POST | `/fs/mkdir` | `{path}` → create directory |
| DELETE | `/fs/dir?path=` | Remove directory |
| POST | `/fs/rename` | `{path, name}` → rename |
| POST | `/fs/move` | `{from, to}` → move |
| POST | `/fs/copy` | `{from, to}` → copy |
| GET | `/fs/stat?path=` | File/dir metadata |
| GET | `/fs/content?path=` | Read text file (max 64KB) |
| PUT | `/fs/content?path=` | Write text file |
| POST | `/fs/upload-stream?path=` | Streaming upload |
| POST | `/fs/upload-session` | Create resumable upload session |
| GET | `/fs/upload-session/:id` | Check upload session status |
| POST | `/fs/upload-chunk/:id` | Upload a chunk (header: `X-Chunk-Index`) |
| DELETE | `/fs/upload-session/:id` | Abort upload session |

---

## Project Structure

New project at `/home/a/projects/minor_project/chttp2/frontend/`

```
frontend/
├── index.html
├── vite.config.ts                   # React plugin + API proxy to :8080
├── components.json                  # shadcn/ui config
├── package.json
├── tsconfig.json / tsconfig.app.json / tsconfig.node.json
└── src/
    ├── main.tsx                     # ReactDOM root + Provider + Router
    ├── App.tsx                      # Route definitions + ProtectedRoute
    ├── index.css                    # Tailwind directives + Plus Jakarta Sans + theme tokens
    │
    ├── lib/
    │   └── utils.ts                 # cn() helper (shadcn standard)
    │
    ├── store/
    │   ├── index.ts                 # configureStore
    │   ├── hooks.ts                 # Typed useAppDispatch / useAppSelector
    │   └── slices/
    │       ├── authSlice.ts         # user, status, loginError
    │       ├── fileSystemSlice.ts   # entries, path, viewMode, sort, selection, clipboard
    │       ├── sessionsSlice.ts     # sessions list
    │       └── uploadsSlice.ts      # upload items with progress
    │
    ├── api/
    │   ├── client.ts                # Base fetch wrapper (credentials: "include", 401 handler)
    │   ├── auth.ts                  # login(), whoami(), logout()
    │   ├── sessions.ts              # getSessions(), deleteSession(), switchSession()
    │   ├── filesystem.ts            # listDir(), mkdir(), deleteFile(), rename(), move(), copy(), stat(), readContent(), writeContent(), downloadFile()
    │   └── uploads.ts               # simpleUpload(), createUploadSession(), uploadChunk(), getUploadStatus(), abortUpload()
    │
    ├── hooks/
    │   ├── useAuth.ts               # Auth state + redirect logic
    │   ├── useFileNavigation.ts     # Path management, breadcrumbs, URL sync
    │   ├── useSelection.ts          # Multi-select: Ctrl+click, Shift+click, select-all
    │   ├── useKeyboardShortcuts.ts  # Delete, F2, Ctrl+C/X/V/A, Enter, arrows
    │   ├── useDragDrop.ts           # Drag-drop file upload onto browser
    │   └── useContextMenu.ts        # Right-click position/state management
    │
    ├── pages/
    │   ├── LoginPage.tsx            # Login form + existing sessions panel
    │   ├── DashboardPage.tsx        # AppShell + FileBrowser
    │   └── SettingsPage.tsx         # whoami info + session management
    │
    ├── components/
    │   ├── ui/                      # shadcn/ui primitives (generated via CLI)
    │   ├── layout/
    │   │   ├── AppShell.tsx         # Sidebar + TopBar + content area
    │   │   ├── Sidebar.tsx          # Nav links: Files, Uploads, Settings
    │   │   └── TopBar.tsx           # Breadcrumbs, user dropdown, logout
    │   ├── auth/
    │   │   └── LoginForm.tsx
    │   ├── files/
    │   │   ├── FileBrowser.tsx      # Orchestrator: breadcrumbs + toolbar + grid/list + context menu
    │   │   ├── Breadcrumbs.tsx      # Clickable path segments with home icon
    │   │   ├── Toolbar.tsx          # View toggle, sort, new folder, upload btn
    │   │   ├── FileGrid.tsx         # Grid layout of FileCards
    │   │   ├── FileList.tsx         # Table layout of FileRows
    │   │   ├── FileCard.tsx         # Grid item: icon, name, size, modified
    │   │   ├── FileRow.tsx          # Table row: checkbox, icon, name, size, modified, actions
    │   │   ├── FileIcon.tsx         # Extension → icon mapper
    │   │   ├── FileContextMenu.tsx  # Right-click actions (open, download, rename, copy, move, delete, info)
    │   │   ├── FilePreview.tsx      # Side panel: text content, image preview, stat info
    │   │   ├── RenameDialog.tsx
    │   │   ├── MoveDialog.tsx       # Directory tree picker for destination
    │   │   ├── NewFolderDialog.tsx
    │   │   └── DeleteConfirmDialog.tsx
    │   ├── uploads/
    │   │   ├── UploadZone.tsx       # Drag-drop overlay
    │   │   ├── UploadManager.tsx    # Floating panel: all active/completed uploads
    │   │   ├── UploadProgressItem.tsx
    │   │   └── ChunkedUploader.ts   # Resumable upload engine (not a component)
    │   └── sessions/
    │       ├── SessionList.tsx
    │       └── SessionCard.tsx
    │
    └── types/
        ├── api.ts                   # Response types matching backend JSON
        └── files.ts                 # FileEntry, FileStat, UploadItem, etc.
```

---

## Pages & Routes

| Route | Component | Auth | Description |
|-------|-----------|------|-------------|
| `/login` | LoginPage | No | Login form + sessions panel (switch between logged-in users) |
| `/` | DashboardPage | Yes | File browser — the main view. URL synced via `?path=` |
| `/settings` | SettingsPage | Yes | User info (whoami) + session management |

**Auth guard**: `ProtectedRoute` wrapper calls `GET /whoami` on mount. 401 → redirect to `/login`. Whoami response cached in `authSlice`.

---

## Redux Slices

### authSlice
```ts
{ user: WhoamiResponse | null, status: 'idle'|'loading'|'authenticated'|'unauthenticated', loginError: string | null }
```
Thunks: `loginThunk`, `whoamiThunk`, `logoutThunk`

### fileSystemSlice
```ts
{ currentPath: string, entries: FileEntry[], status, error, viewMode: 'grid'|'list', 
  sortBy: 'name'|'size'|'modified', sortOrder: 'asc'|'desc', selectedPaths: string[],
  previewFile: FileEntry | null, previewContent: string | null, fileStat: FileStat | null,
  clipboard: { operation: 'copy'|'cut', paths: string[] } | null }
```
Thunks: `listDir`, `createDir`, `deleteFile`, `deleteDir`, `renameFile`, `moveFile`, `copyFile`, `fetchStat`, `fetchContent`, `saveContent`. All mutating thunks auto-refresh `listDir(currentPath)` on success.

### sessionsSlice
```ts
{ sessions: SessionInfo[], status }
```
Thunks: `fetchSessions`, `deleteSession`, `switchSession` (→ also calls `whoamiThunk`)

### uploadsSlice
```ts
{ items: UploadItem[] }
```
Synchronous reducers: `addUpload`, `updateProgress`, `setStatus`, `removeUpload`. Actual upload logic lives in `ChunkedUploader.ts` which dispatches these actions.

---

## API Layer

**`api/client.ts`** — Custom fetch wrapper (not RTK Query, since uploads/downloads need streaming/binary):

```ts
async function apiRequest<T>(path: string, options?: RequestInit): Promise<T>
// - credentials: "include" on every request
// - Dispatches 'auth:unauthorized' event on 401
// - Parses JSON responses, throws ApiError on non-ok
```

**Vite proxy** (`vite.config.ts`):
```ts
server: {
  port: 5173,
  proxy: {
    '/login':    { target: 'http://localhost:8080' },
    '/logout':   { target: 'http://localhost:8080' },
    '/whoami':   { target: 'http://localhost:8080' },
    '/sessions': { target: 'http://localhost:8080' },
    '/fs':       { target: 'http://localhost:8080' },
  }
}
```

---

## File Browser UX

### Navigation
- Click folder → navigate into it. Breadcrumbs to jump to ancestors.
- URL always reflects path: `/?path=Documents/photos` (shareable, bookmarkable)
- Browser back/forward work via react-router search params

### Views
- **Grid**: 4-5 cols desktop, 2-3 tablet, 1-2 mobile. Cards with icon, name, size
- **List**: Full-width table with sortable column headers
- View preference persisted to `localStorage`

### Multi-Select
- Click = single select. Ctrl+Click = toggle. Shift+Click = range. Ctrl+A = all. Esc = deselect
- Bulk action bar appears: "N items selected" + Delete, Move, Copy, Download

### Context Menu
- Right-click file/folder: Open, Download, Rename, Copy, Move, Delete, Get Info
- Right-click empty space: New Folder, Upload, Paste, Refresh

### Keyboard Shortcuts
- `Delete`: delete selected. `F2`: rename. `Ctrl+C/X/V`: clipboard. `Enter`: open. `Backspace`: parent dir. `Ctrl+Shift+N`: new folder

### File Preview (side panel)
- **Text files** (txt, md, json, js, c, py, etc.): fetch `/fs/content`, render with monospace font
- **Images** (jpg, png, gif, svg, webp): inline `<img>` via `/fs/download?path=`
- **Other files**: stat info display (name, size, permissions, modified)

---

## Upload System

### Simple Upload (files < 10MB)
- Drop files on `UploadZone` or click Upload button → native file picker
- `FormData` + `POST /fs/upload?path=<currentPath>`
- Track progress via `XMLHttpRequest` (fetch doesn't support upload progress)

### Resumable Chunked Upload (files ≥ 10MB)
1. Hash chunks client-side (SHA-256 in Web Worker to avoid UI blocking)
2. `POST /fs/upload-session` with manifest `{dest, filename, file_size, chunk_size, chunk_count, chunk_hashes}`
3. Upload chunks (3 in parallel) via `POST /fs/upload-chunk/:id` with `X-Chunk-Index` header
4. **Pause/Resume**: stop sending chunks; on resume, `GET /fs/upload-session/:id` to find incomplete chunks
5. **Abort**: `DELETE /fs/upload-session/:id`
6. Default chunk size: 2MB (8MB for files > 1GB)

### Upload Manager
- Floating panel (bottom-right), collapsible
- Shows all uploads: filename, progress bar, speed, cancel/pause buttons

---

## Design System Implementation

### Theme Tokens (in `src/index.css` via `@theme`)
- **Colors**: Indigo-600 primary, Violet-600 secondary, Slate-50 bg, Slate-900 text
- **Font**: Plus Jakarta Sans (400, 500, 600, 700, 800)
- **Radius**: `rounded-xl` cards, `rounded-lg` inputs, `rounded-full` primary buttons
- **Shadows**: Indigo-tinted colored shadows (`rgba(79,70,229,0.1)`)

### Key Visual Patterns
- **Background**: Dark radial gradient (matching existing `login.html`): `radial-gradient(circle at 20% 20%, #1e293b, transparent 40%), radial-gradient(circle at 80% 80%, #312e81, transparent 40%), #020617`
- **Cards**: `bg-slate-800/80 backdrop-blur-xl border border-white/7 rounded-2xl shadow-[0_20px_60px_rgba(0,0,0,0.6)]`
- **Gradient buttons**: `bg-gradient-to-r from-indigo-600 to-violet-600` with hover lift
- **Hover effects**: `hover:-translate-y-0.5 hover:shadow-lg hover:shadow-indigo-500/20`
- **Blur orbs**: Absolutely positioned large gradient circles with `blur-3xl` for atmospheric depth
- **Gradient text**: `bg-gradient-to-r from-indigo-400 to-violet-400 bg-clip-text text-transparent` on headings

### shadcn/ui Customization
CSS variables mapped to indigo/violet palette. Dark mode as default (matching existing aesthetic).

---

## Implementation Phases

### Phase 1: Scaffold + Auth
- Create Vite project, install deps (tailwindcss, shadcn, redux toolkit, react-router, lucide-react)
- Configure vite proxy, Tailwind theme tokens, Plus Jakarta Sans, shadcn
- Redux store + `authSlice` + `api/client.ts` + `api/auth.ts`
- `LoginPage` + `LoginForm` + `ProtectedRoute`
- `AppShell` + `TopBar` with user info and logout
- **Result**: Working login → authenticated dashboard shell

### Phase 2: File Browser Core
- `api/filesystem.ts` with all FS endpoints
- `fileSystemSlice` with `listDir`, path, viewMode, sort
- `Breadcrumbs`, `Toolbar`, `FileGrid`, `FileCard`, `FileIcon`, `FileList`, `FileRow`
- Folder navigation + URL sync via `?path=`
- **Result**: Browse directories, switch views, sort files

### Phase 3: File Operations
- `NewFolderDialog`, `RenameDialog`, `DeleteConfirmDialog`, `MoveDialog`
- `FileContextMenu` with all actions
- `useSelection` hook for multi-select + bulk actions bar
- Clipboard (copy/cut/paste) via `fileSystemSlice.clipboard`
- File download (blob → object URL → temp anchor)
- **Result**: Full CRUD, context menus, multi-select, downloads

### Phase 4: File Preview
- `FilePreview` side panel (resizable)
- Text preview via `/fs/content` with monospace rendering
- Image preview via `/fs/download` as img src
- `FileStatPanel` for detailed metadata
- **Result**: Inline preview for text/images, file info panel

### Phase 5: Upload System
- Simple upload: `UploadZone` (drag-drop) + `POST /fs/upload`
- `uploadsSlice` + `UploadManager` + `UploadProgressItem`
- `ChunkedUploader` for resumable uploads with Web Worker hashing
- Pause/resume/abort + concurrent queue (3 chunks, 2 files)
- **Result**: Drag-drop upload with progress, resumable chunked uploads

### Phase 6: Sessions & Settings
- `api/sessions.ts` + `sessionsSlice`
- `SettingsPage` with whoami info + `SessionList`/`SessionCard`
- Session switch on login page (sessions panel matching existing UX)
- **Result**: Multi-session management

### Phase 7: Polish
- `useKeyboardShortcuts` hook
- Toast notifications for all operations (shadcn `sonner`)
- Loading skeletons (shadcn `skeleton`)
- Responsive pass: mobile sidebar sheet, touch targets, mobile grid
- Empty states (empty folder, no results)
- Design polish: blur orbs, colored shadows, gradient text, hover lifts, transitions
- **Result**: Production-ready polished dashboard

---

## Production Build

```json
"build": "tsc && vite build --outDir ../www --emptyOutDir"
```

Output goes to `chttp2/www/`. The C backend's catch-all static handler (`CHTTP_STREAM_GET(&srv, "/*", handle_static)`) will serve the SPA. May need a small tweak to `handle_static` to serve `index.html` for non-file paths (SPA fallback).

---

## Verification

1. **Dev**: Run C server on :8080, Vite on :5173. All API calls proxy through.
2. **Login flow**: POST `/login` → cookies set → GET `/whoami` succeeds → redirected to `/`
3. **File browser**: Navigate directories, create/rename/delete files and folders, verify each operation refreshes the listing
4. **Upload**: Drop a file → progress shows → file appears in listing. Test resumable upload with a large file (>10MB), pause/resume.
5. **Download**: Click download → file saves correctly
6. **Preview**: Click text file → content shows in side panel. Click image → renders inline.
7. **Sessions**: Login as two users → switch between them → whoami reflects the active user
8. **Responsive**: Test at 375px, 768px, 1024px, 1440px widths
9. **Production build**: `npm run build`, verify the SPA serves from `www/` via the C backend on :8080
