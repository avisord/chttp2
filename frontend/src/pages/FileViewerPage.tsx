import { useEffect, useState } from 'react'
import { useSearchParams } from 'react-router-dom'
import { readContent } from '@/api/filesystem'
import { getDownloadUrl } from '@/lib/fileTypes'
import { Download, FileText, Loader2 } from 'lucide-react'

export default function FileViewerPage() {
  const [searchParams] = useSearchParams()
  const path = searchParams.get('path') || ''
  const filename = path.split('/').pop() || path

  const [content, setContent] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    if (!path) {
      setError('No file path specified')
      setLoading(false)
      return
    }

    readContent(path)
      .then((res) => setContent(res.content))
      .catch((err) => setError(err.message || 'Failed to load file'))
      .finally(() => setLoading(false))
  }, [path])

  if (loading) {
    return (
      <div className="flex h-screen items-center justify-center bg-slate-50">
        <Loader2 className="h-8 w-8 animate-spin text-slate-400" />
      </div>
    )
  }

  if (error) {
    return (
      <div className="flex h-screen flex-col items-center justify-center gap-4 bg-slate-50">
        <FileText className="h-12 w-12 text-slate-300" />
        <p className="text-sm text-slate-500">{error}</p>
        {path && (
          <a
            href={getDownloadUrl(path)}
            className="inline-flex items-center gap-2 rounded-lg bg-slate-900 px-4 py-2 text-sm font-medium text-white hover:bg-slate-800"
          >
            <Download className="h-4 w-4" />
            Download instead
          </a>
        )}
      </div>
    )
  }

  return (
    <div className="flex h-screen flex-col bg-slate-50">
      <div className="flex items-center justify-between border-b border-slate-200 bg-white px-5 py-3">
        <div className="flex items-center gap-2 text-sm font-medium text-slate-700">
          <FileText className="h-4 w-4 text-slate-400" />
          {filename}
        </div>
        <a
          href={getDownloadUrl(path)}
          className="inline-flex items-center gap-1.5 rounded-md bg-slate-100 px-3 py-1.5 text-xs font-medium text-slate-600 hover:bg-slate-200"
        >
          <Download className="h-3.5 w-3.5" />
          Download
        </a>
      </div>
      <div className="flex-1 overflow-auto p-4">
        <pre className="min-h-full whitespace-pre-wrap break-words rounded-lg border border-slate-200 bg-white p-5 font-mono text-sm leading-relaxed text-slate-800">
          {content}
        </pre>
      </div>
    </div>
  )
}
