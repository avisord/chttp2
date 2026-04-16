/**
 * Resumable chunked upload engine.
 *
 * Mirrors the POC logic: SHA-256 hashing → session creation → chunk upload
 * with exponential-backoff retry, localStorage resume, sequential queue.
 */

import { API_URL } from '@/api/client'

/* ── Constants ────────────────────────────────────────────────────────── */
const CHUNK_SIZE = 4 * 1024 * 1024 // 4 MB
const LS_PFX = 'chttp2-mq-'
const MAX_RETRY = 10

/* ── Types ────────────────────────────────────────────────────────────── */
export type UploadPhase =
  | 'pending'
  | 'hashing'
  | 'uploading'
  | 'complete'
  | 'error'
  | 'aborted'

export interface UploadItemState {
  id: string
  filename: string
  dest: string
  totalSize: number
  phase: UploadPhase
  /** Chunks hashed so far (hashing phase) */
  hashDone: number
  /** Chunks already on server when upload phase started */
  doneAtStart: number
  /** Missing chunks to upload this session */
  uploadTotal: number
  /** Chunks uploaded this session */
  uploadDone: number
  chunkCount: number
  speed: number // bytes/sec
  eta: number // seconds
  error: string | null
}

type Listener = (item: UploadItemState) => void
type CompletionListener = (item: UploadItemState) => void

/* ── Internal item (has non-serialisable fields) ──────────────────────── */
interface InternalItem extends UploadItemState {
  file: File
  uploadId: string | null
  chunkHashes: string[] | null
  startTime: number | null
  aborted: boolean
  ctrl: AbortController | null
}

/* ── localStorage helpers ─────────────────────────────────────────────── */
function lsKey(dest: string, size: number) {
  return `${LS_PFX}${dest}|${size}`
}
function lsSave(item: InternalItem) {
  localStorage.setItem(
    lsKey(item.dest, item.totalSize),
    JSON.stringify({ upload_id: item.uploadId, chunk_hashes: item.chunkHashes }),
  )
}
function lsLoad(dest: string, size: number): { upload_id: string; chunk_hashes: string[] } | null {
  try {
    return JSON.parse(localStorage.getItem(lsKey(dest, size)) || 'null')
  } catch {
    return null
  }
}
function lsDel(item: InternalItem) {
  localStorage.removeItem(lsKey(item.dest, item.totalSize))
}

/* ── Helpers ──────────────────────────────────────────────────────────── */
const range = (n: number) => Array.from({ length: n }, (_, i) => i)
const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms))
function isAbort(e: unknown): boolean {
  return e instanceof DOMException && e.name === 'AbortError'
}
function hexOf(ab: ArrayBuffer): string {
  return Array.from(new Uint8Array(ab))
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('')
}

/* ── Formatting helpers (exported for UI) ─────────────────────────────── */
export function fmtBytes(b: number): string {
  if (b >= 1e9) return (b / 1e9).toFixed(2) + ' GB'
  if (b >= 1e6) return (b / 1e6).toFixed(1) + ' MB'
  if (b >= 1e3) return (b / 1e3).toFixed(0) + ' KB'
  return b + ' B'
}
export function fmtSecs(s: number): string {
  if (!isFinite(s) || s <= 0) return '...'
  if (s >= 3600) return `${Math.floor(s / 3600)}h ${Math.floor((s % 3600) / 60)}m`
  if (s >= 60) return `${Math.floor(s / 60)}m ${Math.floor(s % 60)}s`
  return `${Math.ceil(s)}s`
}

/* ── Upload progress (% across both phases) ───────────────────────────── */
export function uploadPercent(item: UploadItemState): number {
  if (item.phase === 'complete') return 100
  if (item.phase === 'hashing')
    return item.chunkCount > 0 ? Math.round((item.hashDone / item.chunkCount) * 100) : 0
  if (item.phase === 'uploading') {
    const totalDone = item.doneAtStart + item.uploadDone
    return item.chunkCount > 0 ? Math.round((totalDone / item.chunkCount) * 100) : 0
  }
  return 0
}

/* ══════════════════════════════════════════════════════════════════════ */
/*  UploadEngine — singleton queue manager                               */
/* ══════════════════════════════════════════════════════════════════════ */

class UploadEngine {
  private queue: InternalItem[] = []
  private seq = 0
  private running = false
  private paused = false
  private listeners: Set<Listener> = new Set()
  private completionListeners: Set<CompletionListener> = new Set()

  /* ── Subscribe / unsubscribe ──────────────────────────────────────── */
  onChange(fn: Listener) {
    this.listeners.add(fn)
    return () => this.listeners.delete(fn)
  }

  onComplete(fn: CompletionListener) {
    this.completionListeners.add(fn)
    return () => this.completionListeners.delete(fn)
  }

  private notify(item: InternalItem) {
    const state = this.toState(item)
    this.listeners.forEach((fn) => fn(state))
  }

  private notifyComplete(item: InternalItem) {
    const state = this.toState(item)
    this.completionListeners.forEach((fn) => fn(state))
  }

  /* ── Public API ───────────────────────────────────────────────────── */

  /** Enqueue files for upload to `dir` (relative path to directory). Starts queue automatically. */
  addFiles(files: File[], dir: string) {
    for (const file of files) {
      const chunkCount = Math.ceil(file.size / CHUNK_SIZE) || 1
      // Build dest as directory/filename (e.g. "subdir/file.zip" or just "file.zip" for home)
      const dest = dir === '.' || dir === '' ? file.name : `${dir}/${file.name}`
      const item: InternalItem = {
        id: crypto.randomUUID(),
        file,
        filename: file.name,
        dest,
        totalSize: file.size,
        phase: 'pending',
        hashDone: 0,
        doneAtStart: 0,
        uploadTotal: 0,
        uploadDone: 0,
        chunkCount,
        uploadId: null,
        chunkHashes: null,
        startTime: null,
        speed: 0,
        eta: Infinity,
        error: null,
        aborted: false,
        ctrl: null,
      }
      this.queue.push(item)
      this.notify(item)
    }
    if (!this.running) this.startQueue()
  }

  /** Get snapshots of all items. */
  getItems(): UploadItemState[] {
    return this.queue.map((i) => this.toState(i))
  }

  /** Abort or remove an item by id. */
  abort(id: string) {
    const item = this.queue.find((i) => i.id === id)
    if (!item) return

    if (item.phase === 'complete' || item.phase === 'error' || item.phase === 'aborted') {
      this.queue = this.queue.filter((i) => i !== item)
      this.notify(item)
      return
    }

    item.aborted = true
    item.ctrl?.abort()
    if (item.uploadId) {
      fetch(`${API_URL}/fs/upload-session/${item.uploadId}`, {
        method: 'DELETE',
        credentials: 'include',
      }).catch(() => {})
      lsDel(item)
      item.uploadId = null
    }
    item.phase = 'aborted'
    this.notify(item)
  }

  /** Remove completed / errored / aborted items from the list. */
  clearDone() {
    this.queue = this.queue.filter(
      (i) => i.phase !== 'complete' && i.phase !== 'error' && i.phase !== 'aborted',
    )
    // Notify with a dummy to trigger re-render
    this.listeners.forEach((fn) =>
      fn({
        id: '__clear__',
        filename: '',
        dest: '',
        totalSize: 0,
        phase: 'complete',
        hashDone: 0,
        doneAtStart: 0,
        uploadTotal: 0,
        uploadDone: 0,
        chunkCount: 0,
        speed: 0,
        eta: 0,
        error: null,
      }),
    )
  }

  get isPaused() {
    return this.paused
  }
  get isRunning() {
    return this.running
  }

  togglePause() {
    this.paused = !this.paused
  }

  /* ── Queue loop ───────────────────────────────────────────────────── */
  private async startQueue() {
    if (this.running) return
    this.running = true

    while (true) {
      await this.idleWait(null)
      const item = this.queue.find((i) => i.phase === 'pending')
      if (!item) break
      item.ctrl = new AbortController()
      await this.processItem(item)
    }

    this.running = false
    this.paused = false
  }

  /* ── Process one item ─────────────────────────────────────────────── */
  private async processItem(item: InternalItem) {
    try {
      let missingChunks: number[] | null = null

      /* Try to resume from localStorage */
      const saved = lsLoad(item.dest, item.totalSize)
      if (saved?.upload_id) {
        try {
          const r = await fetch(`${API_URL}/fs/upload-session/${saved.upload_id}`, {
            credentials: 'include',
            signal: item.ctrl!.signal,
          })
          if (r.ok) {
            const st = await r.json()
            item.uploadId = saved.upload_id
            item.chunkHashes = saved.chunk_hashes
            const done = new Set<number>(st.received_chunks)
            missingChunks = range(item.chunkCount).filter((i) => !done.has(i))
            item.hashDone = item.chunkCount
          } else {
            lsDel(item)
          }
        } catch (e) {
          if (isAbort(e)) throw e
          lsDel(item)
        }
      }

      /* Hash phase */
      if (!item.uploadId) {
        item.phase = 'hashing'
        item.hashDone = 0
        this.notify(item)

        item.chunkHashes = await this.hashFile(item)
        if (item.aborted) return

        /* Create session */
        const cr = await fetch(`${API_URL}/fs/upload-session`, {
          method: 'POST',
          credentials: 'include',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            dest: item.dest,
            filename: item.filename,
            file_size: item.totalSize,
            chunk_size: CHUNK_SIZE,
            chunk_count: item.chunkCount,
            chunk_hashes: item.chunkHashes,
          }),
          signal: item.ctrl!.signal,
        })
        if (!cr.ok) throw new Error(await cr.text())
        const { upload_id } = await cr.json()
        item.uploadId = upload_id
        lsSave(item)
        missingChunks = range(item.chunkCount)
      }

      /* Upload phase */
      item.phase = 'uploading'
      item.doneAtStart = item.chunkCount - missingChunks!.length
      item.uploadTotal = missingChunks!.length
      item.uploadDone = 0
      item.startTime = Date.now()
      item.speed = 0
      item.eta = Infinity
      this.notify(item)

      for (const ci of missingChunks!) {
        await this.idleWait(item)
        if (item.aborted) return

        const complete = await this.uploadChunk(item, ci)
        item.uploadDone++
        this.recalcSpeed(item)
        this.notify(item)

        if (complete) {
          item.phase = 'complete'
          item.speed = 0
          item.eta = 0
          lsDel(item)
          this.notify(item)
          this.notifyComplete(item)
          return
        }
      }

      /* All chunks already done */
      item.phase = 'complete'
      lsDel(item)
      this.notify(item)
      this.notifyComplete(item)
    } catch (e) {
      if (isAbort(e)) {
        item.phase = 'aborted'
      } else {
        item.phase = 'error'
        item.error = e instanceof Error ? e.message : String(e)
      }
      this.notify(item)
    }
  }

  /* ── Hash file chunk by chunk ─────────────────────────────────────── */
  private async hashFile(item: InternalItem): Promise<string[]> {
    const hashes: string[] = []
    for (let i = 0; i < item.chunkCount; i++) {
      await this.idleWait(item)
      if (item.aborted) throw new DOMException('Aborted', 'AbortError')
      const ab = await item.file.slice(i * CHUNK_SIZE, (i + 1) * CHUNK_SIZE).arrayBuffer()
      const dg = await crypto.subtle.digest('SHA-256', ab)
      hashes.push(hexOf(dg))
      item.hashDone = i + 1
      this.notify(item)
    }
    return hashes
  }

  /* ── Upload one chunk with retry ──────────────────────────────────── */
  private async uploadChunk(item: InternalItem, ci: number): Promise<boolean> {
    const blob = item.file.slice(ci * CHUNK_SIZE, (ci + 1) * CHUNK_SIZE)
    let delay = 600

    for (let attempt = 0; attempt <= MAX_RETRY; attempt++) {
      await this.idleWait(item)
      if (item.aborted) throw new DOMException('Aborted', 'AbortError')

      try {
        const r = await fetch(`${API_URL}/fs/upload-chunk/${item.uploadId}`, {
          method: 'POST',
          credentials: 'include',
          headers: {
            'Content-Type': 'application/octet-stream',
            'X-Chunk-Index': String(ci),
          },
          body: blob,
          signal: item.ctrl!.signal,
        })
        if (r.status === 422) {
          if (attempt < MAX_RETRY) {
            await sleep(delay)
            delay = Math.min(delay * 2, 30000)
            continue
          }
          throw new Error(`Hash mismatch on chunk ${ci}`)
        }
        if (!r.ok) {
          if (attempt < MAX_RETRY) {
            await sleep(delay)
            delay = Math.min(delay * 2, 30000)
            continue
          }
          throw new Error(`HTTP ${r.status} uploading chunk ${ci}`)
        }
        const body = await r.json()
        return !!body.path // true = server signalled completion
      } catch (e) {
        if (isAbort(e)) throw e
        if (attempt < MAX_RETRY) {
          await sleep(delay)
          delay = Math.min(delay * 2, 30000)
        } else throw e
      }
    }
    return false
  }

  /* ── Speed / ETA ──────────────────────────────────────────────────── */
  private recalcSpeed(item: InternalItem) {
    const elapsed = (Date.now() - (item.startTime || Date.now())) / 1000
    if (elapsed < 0.2) return
    const bytesDone = item.uploadDone * CHUNK_SIZE
    item.speed = bytesDone / elapsed
    const bytesLeft = (item.uploadTotal - item.uploadDone) * CHUNK_SIZE
    item.eta = item.speed > 0 ? bytesLeft / item.speed : Infinity
  }

  /* ── Idle / pause wait ────────────────────────────────────────────── */
  private async idleWait(item: InternalItem | null) {
    while (this.paused) {
      if (item?.aborted) return
      await sleep(50)
    }
  }

  /* ── Serialise to public state ────────────────────────────────────── */
  private toState(item: InternalItem): UploadItemState {
    return {
      id: item.id,
      filename: item.filename,
      dest: item.dest,
      totalSize: item.totalSize,
      phase: item.phase,
      hashDone: item.hashDone,
      doneAtStart: item.doneAtStart,
      uploadTotal: item.uploadTotal,
      uploadDone: item.uploadDone,
      chunkCount: item.chunkCount,
      speed: item.speed,
      eta: item.eta,
      error: item.error,
    }
  }
}

/* ── Singleton ────────────────────────────────────────────────────────── */
export const uploadEngine = new UploadEngine()
