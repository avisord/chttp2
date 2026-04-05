import { useAppSelector } from '@/store/hooks'
import SessionList from '@/components/sessions/SessionList'

export default function SettingsPage() {
  const user = useAppSelector((s) => s.auth.user)

  return (
    <div className="mx-auto max-w-2xl space-y-8 px-4 py-8 sm:px-6">
      {/* User info */}
      <section>
        <h2 className="mb-1 text-xl font-extrabold tracking-tight text-slate-900">
          User <span className="bg-gradient-to-r from-indigo-600 to-violet-600 bg-clip-text text-transparent">Info</span>
        </h2>
        <p className="mb-5 text-sm text-slate-500">Your system account details</p>
        <div className="rounded-xl border border-slate-100 bg-white p-5 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)]">
          {user ? (
            <div className="space-y-0">
              {(
                [
                  ['Username', user.username],
                  ['UID', user.uid],
                  ['GID', user.gid],
                  ['Home', user.home],
                  ['Shell', user.shell],
                  ['CWD', user.cwd],
                ] as const
              ).map(([label, value]) => (
                <div key={label} className="flex items-baseline justify-between border-b border-slate-100 py-3 last:border-0">
                  <span className="text-sm font-medium text-slate-500">{label}</span>
                  <span className="font-mono text-sm text-slate-900">{String(value)}</span>
                </div>
              ))}
            </div>
          ) : (
            <p className="text-sm text-slate-400">Loading...</p>
          )}
        </div>
      </section>

      {/* Sessions */}
      <section>
        <h2 className="mb-1 text-xl font-extrabold tracking-tight text-slate-900">
          Active <span className="bg-gradient-to-r from-indigo-600 to-violet-600 bg-clip-text text-transparent">Sessions</span>
        </h2>
        <p className="mb-5 text-sm text-slate-500">Manage your authenticated sessions</p>
        <SessionList />
      </section>
    </div>
  )
}
