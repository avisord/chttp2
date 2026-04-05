import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from '@/components/ui/alert-dialog'

interface DeleteConfirmDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  names: string[]
  onConfirm: () => void
}

export default function DeleteConfirmDialog({ open, onOpenChange, names, onConfirm }: DeleteConfirmDialogProps) {
  return (
    <AlertDialog open={open} onOpenChange={onOpenChange}>
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>Delete {names.length > 1 ? `${names.length} items` : `"${names[0]}"`}?</AlertDialogTitle>
          <AlertDialogDescription>
            This action cannot be undone.
            {names.length > 1 && (
              <span className="mt-2 block text-xs text-muted-foreground">
                {names.slice(0, 5).join(', ')}
                {names.length > 5 && ` and ${names.length - 5} more`}
              </span>
            )}
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>Cancel</AlertDialogCancel>
          <AlertDialogAction
            onClick={onConfirm}
            className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
          >
            Delete
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  )
}
