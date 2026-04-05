import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
  DialogFooter,
  DialogClose,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Download, FileQuestion } from 'lucide-react'

interface DownloadDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  filename: string
  onDownload: () => void
}

export default function DownloadDialog({ open, onOpenChange, filename, onDownload }: DownloadDialogProps) {
  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Cannot preview file</DialogTitle>
          <DialogDescription>
            <span className="font-medium text-slate-700">{filename}</span> cannot be previewed in the browser. Would you like to download it instead?
          </DialogDescription>
        </DialogHeader>
        <div className="flex justify-center py-4">
          <div className="flex h-16 w-16 items-center justify-center rounded-xl bg-slate-100">
            <FileQuestion className="h-8 w-8 text-slate-400" />
          </div>
        </div>
        <DialogFooter>
          <DialogClose render={<Button variant="outline" />}>Cancel</DialogClose>
          <Button
            onClick={() => {
              onDownload()
              onOpenChange(false)
            }}
          >
            <Download className="mr-2 h-4 w-4" />
            Download
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
