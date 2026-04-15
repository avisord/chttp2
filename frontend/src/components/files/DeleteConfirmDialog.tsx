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
  permanent?: boolean
}

export default function DeleteConfirmDialog({ open, onOpenChange, names, onConfirm, permanent }: DeleteConfirmDialogProps) {
  const itemLabel = names.length > 1 ? `${names.length} items` : `"${names[0]}"`

  return (
    <AlertDialog open={open} onOpenChange={onOpenChange}>
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>
            {permanent ? `Permanently delete ${itemLabel}?` : `Move ${itemLabel} to Trash?`}
          </AlertDialogTitle>
          <AlertDialogDescription>
            {permanent
              ? 'This action cannot be undone.'
              : 'You can restore it later from Trash.'}
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
            {permanent ? 'Delete Permanently' : 'Move to Trash'}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  )
}
