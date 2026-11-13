<script lang="ts">
	import { onMount } from 'svelte';
	import * as Table from '$lib/components/ui/table/index.js';
	import * as Dialog from '$lib/components/ui/dialog/index.js';
	import * as Select from '$lib/components/ui/select/index.js';
	import * as Alert from '$lib/components/ui/alert/index.js';
	import { ScrollArea } from '$lib/components/ui/scroll-area/index.js';
	import { Button } from '$lib/components/ui/button/index.js';
	import { Input } from '$lib/components/ui/input/index.js';
	import { Label } from '$lib/components/ui/label/index.js';
	import { Badge } from '$lib/components/ui/badge/index.js';
	import UserPlus from '@lucide/svelte/icons/user-plus';
	import Pencil from '@lucide/svelte/icons/pencil';
	import KeyRound from '@lucide/svelte/icons/key-round';
	import Trash2 from '@lucide/svelte/icons/trash-2';
	import LogOut from '@lucide/svelte/icons/log-out';
	import RefreshCw from '@lucide/svelte/icons/refresh-cw';
	import Search from '@lucide/svelte/icons/search';
	import CircleAlert from '@lucide/svelte/icons/circle-alert';
	import UserRound from '@lucide/svelte/icons/user-round';

	import { useModuleTick } from '$lib/amcf/poll.svelte';
	import { paginate } from '$lib/amcf/paging';
	import ListPager from '$lib/amcf/ListPager.svelte';
	import {
		getUsersMeta,
		listUsers,
		getOwnUser,
		getUserSessions,
		createUser,
		updateUser,
		resetUserPassword,
		deleteUser,
		revokeUserSessions,
		updateOwnProfile,
		changeOwnPassword,
		checkPasswordPolicy,
		describeUserError,
		type UsersMeta,
		type ManagedUser,
		type UserRole,
		type UserSession,
		type UserUpdate
	} from '$lib/amcf/usersApi';

	type DialogMode = 'none' | 'create' | 'edit' | 'password' | 'delete' | 'ownpassword' | 'ownprofile' | 'sessions';

	let { module, app }: { module: any; app: any } = $props();
	const poll = useModuleTick(() => module);

	// Widget options can only hide features; the server decides what is allowed.
	let visible = $derived.by(() => { poll.v; return module.visible !== false; });
	let title = $derived.by(() => { poll.v; return String(module.title || ''); });
	let selfMode = $derived.by(() => { poll.v; return module.mode === 'self'; });
	let columns = $derived.by(() => {
		poll.v;
		return String(module.columns || '')
			.split(',')
			.map((column) => column.trim().toLowerCase())
			.filter((column) => column !== '');
	});
	let roleFilter = $derived.by(() => {
		poll.v;
		return String(module.roles || '')
			.split(',')
			.map((role) => role.trim())
			.filter((role) => role !== '');
	});
	let entriesPerPage = $derived.by(() => { poll.v; return Number(module.entriesperpage) > 0 ? Number(module.entriesperpage) : 25; });
	let options = $derived.by(() => {
		poll.v;
		return {
			create: module.allowcreate !== false,
			edit: module.allowedit !== false,
			roleEdit: module.allowroleedit !== false,
			passwordReset: module.allowpasswordreset !== false,
			delete: module.allowdelete !== false,
			sessions: module.allowsessions !== false,
			ownPassword: module.allowownpassword !== false,
			showRolePermissions: module.showrolepermissions !== false,
			confirmDelete: module.confirmdelete !== false
		};
	});

	let meta = $state.raw<UsersMeta | null>(null);
	let users = $state.raw<ManagedUser[]>([]);
	let ownUser = $state.raw<ManagedUser | null>(null);
	let serverTime = $state(0);
	let loading = $state(true);
	let loadError = $state('');
	let notice = $state('');

	let searchText = $state('');
	let pageNumber = $state(1);

	let dialogMode = $state<DialogMode>('none');
	let dialogTarget = $state.raw<ManagedUser | null>(null);
	let dialogError = $state('');
	let busy = $state(false);
	let sessions = $state.raw<UserSession[]>([]);

	let formLogin = $state('');
	let formDescription = $state('');
	let formRole = $state('');
	let formLanguage = $state('');
	let formPassword = $state('');
	let formPasswordConfirm = $state('');
	let formCurrentPassword = $state('');

	let can = $derived.by(() => {
		const capabilities = meta?.capabilities;
		return {
			view: capabilities?.viewusers === true,
			create: options.create && capabilities?.createuser === true,
			edit: options.edit && capabilities?.editdetails === true,
			assignRole: options.roleEdit && capabilities?.assignrole === true,
			resetPassword: options.passwordReset && capabilities?.resetpassword === true,
			delete: options.delete && capabilities?.deleteuser === true,
			sessions: options.sessions && capabilities?.managesessions === true,
			ownPassword: options.ownPassword && capabilities?.changeownpassword === true,
			ownProfile: options.edit && capabilities?.editownprofile === true
		};
	});

	let roleOptions = $derived.by(() =>
		(meta?.roles || []).filter(
			(role) => role.assignable && !role.deprecated && (roleFilter.length === 0 || roleFilter.includes(role.identifier))
		)
	);

	let filteredUsers = $derived.by(() => {
		const needle = searchText.trim().toLowerCase();
		if (needle === '') return users;
		return users.filter(
			(user) =>
				user.login.toLowerCase().includes(needle) ||
				user.description.toLowerCase().includes(needle) ||
				user.roleName.toLowerCase().includes(needle)
		);
	});
	let listPage = $derived(paginate(filteredUsers, pageNumber, entriesPerPage));
	let onlineCount = $derived(users.filter((user) => user.online).length);

	let passwordProblems = $derived(meta ? checkPasswordPolicy(formPassword, meta.passwordPolicy) : []);
	let passwordsMatch = $derived(formPassword === formPasswordConfirm);
	let selectedRole = $derived((meta?.roles || []).find((role) => role.identifier === formRole) || null);

	function roleName(identifier: string): string {
		const role = (meta?.roles || []).find((entry) => entry.identifier === identifier);
		return role ? role.displayName : identifier;
	}

	function languageName(identifier: string): string {
		if (!identifier) return '';
		const language = (meta?.languages || []).find((entry) => entry.identifier === identifier);
		return language ? language.displayName : identifier;
	}

	function permissionName(identifier: string): string {
		const permission = (meta?.permissions || []).find((entry) => entry.identifier === identifier);
		return permission ? permission.displayName : identifier;
	}

	function idleMinutes(user: ManagedUser): number {
		if (!user.online || user.lastActivity <= 0 || serverTime <= 0) return 0;
		return Math.max(0, Math.floor((serverTime - user.lastActivity) / 60000000));
	}

	function formatActivity(timestamp: number): string {
		if (timestamp <= 0) return '';
		return new Date(timestamp / 1000).toLocaleString([], { dateStyle: 'short', timeStyle: 'short' });
	}

	async function reload() {
		if (!app) return;
		loading = true;
		loadError = '';
		try {
			meta = await getUsersMeta(app);
			if (selfMode || !meta.capabilities.viewusers) {
				ownUser = await getOwnUser(app);
				users = [];
			}
			else {
				const list = await listUsers(app);
				users = list.users;
				serverTime = list.serverTime;
			}
		}
		catch (err) {
			loadError = describeUserError(app, err);
		}
		finally {
			loading = false;
		}
	}

	onMount(() => {
		reload();
	});

	function resetForm() {
		formLogin = '';
		formDescription = '';
		formRole = '';
		formLanguage = '';
		formPassword = '';
		formPasswordConfirm = '';
		formCurrentPassword = '';
		dialogError = '';
	}

	function openDialog(mode: DialogMode, target: ManagedUser | null) {
		resetForm();
		dialogTarget = target;
		notice = '';
		if (mode === 'create') {
			const defaultRole = roleOptions.find((role) => role.isDefault) || roleOptions[0];
			formRole = defaultRole ? defaultRole.identifier : '';
			formLanguage = meta?.languages[0]?.identifier || '';
		}
		if ((mode === 'edit' || mode === 'ownprofile') && target) {
			formDescription = target.description;
			formRole = target.role;
			formLanguage = target.language;
		}
		dialogMode = mode;
		if (mode === 'sessions' && target) loadSessions(target);
	}

	function closeDialog() {
		dialogMode = 'none';
		dialogTarget = null;
		sessions = [];
		resetForm();
	}

	async function loadSessions(target: ManagedUser) {
		try {
			sessions = await getUserSessions(app, target.uuid);
		}
		catch (err) {
			dialogError = describeUserError(app, err);
		}
	}

	// Runs a server operation for the open dialog, then reloads the list.
	async function submit(action: () => Promise<void>, successText: string) {
		busy = true;
		dialogError = '';
		try {
			await action();
			closeDialog();
			notice = successText;
			await reload();
		}
		catch (err) {
			dialogError = describeUserError(app, err);
		}
		finally {
			busy = false;
		}
	}

	function newPasswordIsValid(): boolean {
		if (passwordProblems.length > 0) {
			dialogError = 'The password needs ' + passwordProblems.join(', ') + '.';
			return false;
		}
		if (!passwordsMatch) {
			dialogError = 'The passwords do not match.';
			return false;
		}
		return true;
	}

	function submitCreate() {
		if (formLogin.trim() === '') {
			dialogError = 'Please enter a login.';
			return;
		}
		if (!newPasswordIsValid()) return;
		const login = formLogin.trim();
		submit(async () => {
			await createUser(app, login, formDescription.trim(), formRole, formLanguage, formPassword);
		}, `User ${login} has been created.`);
	}

	function submitEdit() {
		const target = dialogTarget;
		if (!target) return;
		const update: UserUpdate = {};
		if (formDescription.trim() !== target.description) update.description = formDescription.trim();
		if (formLanguage !== target.language) update.language = formLanguage;
		if (can.assignRole && !target.isSelf && formRole !== target.role) update.role = formRole;
		if (Object.keys(update).length === 0) {
			closeDialog();
			return;
		}
		const text = update.role !== undefined ? `User ${target.login} has been updated and must log in again.` : `User ${target.login} has been updated.`;
		submit(async () => {
			await updateUser(app, target.uuid, update);
		}, text);
	}

	function submitOwnProfile() {
		const target = dialogTarget;
		if (!target) return;
		const update: { description?: string; language?: string } = {};
		if (formDescription.trim() !== target.description) update.description = formDescription.trim();
		if (formLanguage !== target.language) update.language = formLanguage;
		if (Object.keys(update).length === 0) {
			closeDialog();
			return;
		}
		submit(async () => {
			await updateOwnProfile(app, update);
		}, 'Your profile has been updated.');
	}

	function submitPasswordReset() {
		const target = dialogTarget;
		if (!target || !newPasswordIsValid()) return;
		submit(async () => {
			await resetUserPassword(app, target.uuid, formPassword);
		}, `The password of ${target.login} has been reset.`);
	}

	function submitOwnPassword() {
		if (formCurrentPassword === '') {
			dialogError = 'Please enter your current password.';
			return;
		}
		if (!newPasswordIsValid()) return;
		submit(async () => {
			await changeOwnPassword(app, formCurrentPassword, formPassword);
		}, 'Your password has been changed.');
	}

	function submitDelete(target: ManagedUser) {
		submit(async () => {
			await deleteUser(app, target.uuid);
		}, `User ${target.login} has been deleted.`);
	}

	function requestDelete(target: ManagedUser) {
		if (options.confirmDelete) openDialog('delete', target);
		else submitDelete(target);
	}

	function submitRevoke() {
		const target = dialogTarget;
		if (!target) return;
		submit(async () => {
			const count = await revokeUserSessions(app, target.uuid);
			if (count === 0) throw new Error('No sessions to end.');
		}, `The sessions of ${target.login} have been ended.`);
	}

	function hasColumn(name: string): boolean {
		return columns.includes(name);
	}

	let columnCount = $derived(columns.length + 1);
</script>

{#snippet roleSelect(roles: UserRole[], disabled: boolean)}
	<Select.Root type="single" value={formRole} onValueChange={(value: string) => { formRole = value; }} {disabled}>
		<Select.Trigger class="h-9 w-full text-sm">
			{formRole ? roleName(formRole) : 'Select a role'}
		</Select.Trigger>
		<Select.Content>
			{#each roles as role (role.identifier)}
				<Select.Item value={role.identifier}>{role.displayName}</Select.Item>
			{/each}
		</Select.Content>
	</Select.Root>
	{#if options.showRolePermissions && selectedRole}
		<div class="flex flex-wrap gap-1 pt-1">
			{#each selectedRole.permissions as permission (permission)}
				<Badge variant="secondary" class="text-[10px] font-normal">{permissionName(permission)}</Badge>
			{:else}
				<span class="text-xs text-muted-foreground">This role has no permissions.</span>
			{/each}
		</div>
	{/if}
{/snippet}

{#snippet languageSelect()}
	{#if meta && meta.languages.length > 0}
		<Select.Root type="single" value={formLanguage} onValueChange={(value: string) => { formLanguage = value; }}>
			<Select.Trigger class="h-9 w-full text-sm">
				{formLanguage ? languageName(formLanguage) : 'Default'}
			</Select.Trigger>
			<Select.Content>
				{#each meta.languages as language (language.identifier)}
					<Select.Item value={language.identifier}>{language.displayName}</Select.Item>
				{/each}
			</Select.Content>
		</Select.Root>
	{/if}
{/snippet}

{#snippet passwordFields(withCurrent: boolean)}
	{#if withCurrent}
		<div class="grid gap-1.5">
			<Label for="um-current-password">Current password</Label>
			<Input id="um-current-password" type="password" autocomplete="current-password" bind:value={formCurrentPassword} />
		</div>
	{/if}
	<div class="grid gap-1.5">
		<Label for="um-password">New password</Label>
		<Input id="um-password" type="password" autocomplete="new-password" bind:value={formPassword} />
		{#if formPassword !== '' && passwordProblems.length > 0}
			<span class="text-xs text-muted-foreground">Needs {passwordProblems.join(', ')}.</span>
		{/if}
	</div>
	<div class="grid gap-1.5">
		<Label for="um-password-confirm">Repeat new password</Label>
		<Input id="um-password-confirm" type="password" autocomplete="new-password" bind:value={formPasswordConfirm} />
		{#if formPasswordConfirm !== '' && !passwordsMatch}
			<span class="text-xs text-destructive">The passwords do not match.</span>
		{/if}
	</div>
{/snippet}

{#if visible}
	<div class="w-full flex-1 min-h-0 flex flex-col gap-3">
		{#if loadError}
			<Alert.Root variant="destructive">
				<CircleAlert />
				<Alert.Title>Users could not be loaded</Alert.Title>
				<Alert.Description>{loadError}</Alert.Description>
			</Alert.Root>
		{/if}
		{#if notice}
			<div class="text-sm rounded-md border bg-muted/40 px-3 py-2 flex items-center justify-between gap-2">
				<span>{notice}</span>
				<button type="button" class="text-xs text-muted-foreground hover:text-foreground" onclick={() => { notice = ''; }}>Dismiss</button>
			</div>
		{/if}

		{#if selfMode || (meta && !can.view)}
			<!-- Own account only -->
			{#if ownUser}
				<div class="rounded-md border p-4 flex flex-col gap-3">
					<div class="flex items-center gap-3">
						<div class="h-10 w-10 rounded-full bg-muted flex items-center justify-center">
							<UserRound class="h-5 w-5 text-muted-foreground" />
						</div>
						<div class="min-w-0">
							<div class="font-medium">{ownUser.login}</div>
							<div class="text-xs text-muted-foreground truncate">{ownUser.description}</div>
						</div>
					</div>
					<div class="grid grid-cols-[110px_1fr] gap-y-1.5 text-sm">
						<span class="text-muted-foreground">Role</span><span>{ownUser.roleName}</span>
						{#if ownUser.language}
							<span class="text-muted-foreground">Language</span><span>{languageName(ownUser.language)}</span>
						{/if}
						<span class="text-muted-foreground">Sessions</span><span>{ownUser.sessionCount}</span>
					</div>
					<div class="flex flex-wrap gap-2">
						{#if can.ownProfile}
							<Button variant="outline" size="sm" onclick={() => openDialog('ownprofile', ownUser)}>
								<Pencil class="h-4 w-4" /> Edit profile
							</Button>
						{/if}
						{#if can.ownPassword}
							<Button variant="outline" size="sm" onclick={() => openDialog('ownpassword', ownUser)}>
								<KeyRound class="h-4 w-4" /> Change password
							</Button>
						{/if}
					</div>
				</div>
			{:else if loading}
				<div class="text-sm text-muted-foreground">Loading…</div>
			{/if}
		{:else}
			{#if meta?.defaultTestUser}
				<div class="flex items-start gap-2 rounded-md border border-amber-300 dark:border-amber-800 bg-amber-50 dark:bg-amber-950/40 px-3 py-2 text-sm text-amber-800 dark:text-amber-300">
					<CircleAlert class="h-4 w-4 mt-0.5 shrink-0" />
					<span>The default account <span class="font-mono">test</span> with password <span class="font-mono">test</span> still exists. Delete it or change its password.</span>
				</div>
			{/if}

			<!-- Toolbar -->
			<div class="flex flex-wrap items-center justify-between gap-2">
				<div class="flex items-baseline gap-2 min-w-0">
					{#if title}<span class="font-semibold">{title}</span>{/if}
					<span class="text-xs text-muted-foreground">{users.length} {users.length === 1 ? 'user' : 'users'} · {onlineCount} online</span>
				</div>
				<div class="flex items-center gap-2">
					<div class="relative">
						<Search class="h-4 w-4 absolute left-2 top-1/2 -translate-y-1/2 text-muted-foreground pointer-events-none" />
						<Input class="h-8 w-48 pl-8 text-sm" placeholder="Search…" bind:value={searchText} oninput={() => { pageNumber = 1; }} />
					</div>
					<Button variant="ghost" size="sm" class="h-8" onclick={reload} disabled={loading} title="Reload">
						<RefreshCw class={loading ? 'h-4 w-4 animate-spin' : 'h-4 w-4'} />
					</Button>
					{#if can.create}
						<Button size="sm" class="h-8" onclick={() => openDialog('create', null)}>
							<UserPlus class="h-4 w-4" /> New user
						</Button>
					{/if}
				</div>
			</div>

			<div class="flex-1 min-h-0 flex flex-col border rounded-md overflow-hidden">
				<ScrollArea class="flex-1 min-h-0">
					<Table.Root>
						<Table.Header class="sticky top-0 bg-muted z-10">
							<Table.Row>
								{#if hasColumn('login')}<Table.Head class="text-xs font-semibold uppercase tracking-wider">Login</Table.Head>{/if}
								{#if hasColumn('description')}<Table.Head class="text-xs font-semibold uppercase tracking-wider">Description</Table.Head>{/if}
								{#if hasColumn('role')}<Table.Head class="text-xs font-semibold uppercase tracking-wider">Role</Table.Head>{/if}
								{#if hasColumn('language')}<Table.Head class="text-xs font-semibold uppercase tracking-wider">Language</Table.Head>{/if}
								{#if hasColumn('online')}<Table.Head class="text-xs font-semibold uppercase tracking-wider">Status</Table.Head>{/if}
								{#if hasColumn('lastactivity')}<Table.Head class="text-xs font-semibold uppercase tracking-wider">Last activity</Table.Head>{/if}
								<Table.Head class="w-[1%]"></Table.Head>
							</Table.Row>
						</Table.Header>
						<Table.Body>
							{#each listPage.rows as user (user.uuid)}
								<Table.Row class="hover:bg-muted/50 transition-colors">
									{#if hasColumn('login')}
										<Table.Cell class="text-sm py-1.5 font-medium">
											{user.login}
											{#if user.isSelf}<Badge variant="outline" class="ml-1 text-[10px]">you</Badge>{/if}
										</Table.Cell>
									{/if}
									{#if hasColumn('description')}<Table.Cell class="text-sm py-1.5 text-muted-foreground">{user.description}</Table.Cell>{/if}
									{#if hasColumn('role')}<Table.Cell class="text-sm py-1.5">{user.roleName}</Table.Cell>{/if}
									{#if hasColumn('language')}<Table.Cell class="text-sm py-1.5">{languageName(user.language)}</Table.Cell>{/if}
									{#if hasColumn('online')}
										<Table.Cell class="text-sm py-1.5">
											{#if !user.online}
												<span class="text-xs text-muted-foreground">offline</span>
											{:else if idleMinutes(user) < 2}
												<Badge class="text-[10px] bg-emerald-600 text-white border-transparent">online</Badge>
											{:else}
												<Badge variant="secondary" class="text-[10px]">idle {idleMinutes(user)} min</Badge>
											{/if}
										</Table.Cell>
									{/if}
									{#if hasColumn('lastactivity')}<Table.Cell class="text-xs py-1.5 text-muted-foreground">{formatActivity(user.lastActivity)}</Table.Cell>{/if}
									<Table.Cell class="py-1.5">
										<div class="flex justify-end gap-1">
											{#if user.isSelf}
												{#if can.edit || can.ownProfile}
													<Button variant="ghost" size="sm" class="h-7 px-2" title="Edit" onclick={() => openDialog('edit', user)}><Pencil class="h-4 w-4" /></Button>
												{/if}
												{#if can.ownPassword}
													<Button variant="ghost" size="sm" class="h-7 px-2" title="Change my password" onclick={() => openDialog('ownpassword', user)}><KeyRound class="h-4 w-4" /></Button>
												{/if}
											{:else if user.manageable}
												{#if can.edit || can.assignRole}
													<Button variant="ghost" size="sm" class="h-7 px-2" title="Edit" onclick={() => openDialog('edit', user)}><Pencil class="h-4 w-4" /></Button>
												{/if}
												{#if can.resetPassword}
													<Button variant="ghost" size="sm" class="h-7 px-2" title="Reset password" onclick={() => openDialog('password', user)}><KeyRound class="h-4 w-4" /></Button>
												{/if}
												{#if can.sessions && user.online}
													<Button variant="ghost" size="sm" class="h-7 px-2" title="Sessions" onclick={() => openDialog('sessions', user)}><LogOut class="h-4 w-4" /></Button>
												{/if}
												{#if can.delete}
													<Button variant="ghost" size="sm" class="h-7 px-2 text-destructive hover:text-destructive" title="Delete" onclick={() => requestDelete(user)}><Trash2 class="h-4 w-4" /></Button>
												{/if}
											{/if}
										</div>
									</Table.Cell>
								</Table.Row>
							{:else}
								<Table.Row>
									<Table.Cell colspan={columnCount} class="text-center py-8 text-muted-foreground text-sm">
										{loading ? 'Loading…' : searchText ? 'No users match the search.' : 'No users.'}
									</Table.Cell>
								</Table.Row>
							{/each}
						</Table.Body>
					</Table.Root>
				</ScrollArea>
				<ListPager {listPage} onPageChange={(page) => { pageNumber = page; }} />
			</div>
		{/if}
	</div>

	<Dialog.Root open={dialogMode !== 'none'} onOpenChange={(open) => { if (!open && !busy) closeDialog(); }}>
		<Dialog.Content class="sm:max-w-[460px]">
			{#if dialogMode === 'create'}
				<Dialog.Header>
					<Dialog.Title>New user</Dialog.Title>
					<Dialog.Description>The user can log in as soon as the account exists.</Dialog.Description>
				</Dialog.Header>
				<div class="grid gap-3 py-2">
					<div class="grid gap-1.5">
						<Label for="um-login">Login</Label>
						<Input id="um-login" autocomplete="off" bind:value={formLogin} />
					</div>
					<div class="grid gap-1.5">
						<Label for="um-description">Description</Label>
						<Input id="um-description" bind:value={formDescription} />
					</div>
					<div class="grid gap-1.5">
						<Label>Role</Label>
						{@render roleSelect(roleOptions, !can.assignRole)}
					</div>
					{#if meta && meta.languages.length > 0}
						<div class="grid gap-1.5">
							<Label>Language</Label>
							{@render languageSelect()}
						</div>
					{/if}
					{@render passwordFields(false)}
				</div>
			{:else if dialogMode === 'edit' && dialogTarget}
				<Dialog.Header>
					<Dialog.Title>Edit {dialogTarget.login}</Dialog.Title>
					<Dialog.Description>
						{dialogTarget.isSelf ? 'You can not change your own role.' : 'Changing the role logs the user out.'}
					</Dialog.Description>
				</Dialog.Header>
				<div class="grid gap-3 py-2">
					<div class="grid gap-1.5">
						<Label for="um-edit-description">Description</Label>
						<Input id="um-edit-description" bind:value={formDescription} disabled={!can.edit && !(dialogTarget.isSelf && can.ownProfile)} />
					</div>
					<div class="grid gap-1.5">
						<Label>Role</Label>
						{#if can.assignRole && !dialogTarget.isSelf}
							{@render roleSelect(roleOptions, false)}
						{:else}
							{@render roleSelect([], true)}
						{/if}
					</div>
					{#if meta && meta.languages.length > 0 && (can.edit || (dialogTarget.isSelf && can.ownProfile))}
						<div class="grid gap-1.5">
							<Label>Language</Label>
							{@render languageSelect()}
						</div>
					{/if}
				</div>
			{:else if dialogMode === 'ownprofile' && dialogTarget}
				<Dialog.Header>
					<Dialog.Title>Edit profile</Dialog.Title>
					<Dialog.Description>Your role can only be changed by a user manager.</Dialog.Description>
				</Dialog.Header>
				<div class="grid gap-3 py-2">
					<div class="grid gap-1.5">
						<Label for="um-own-description">Description</Label>
						<Input id="um-own-description" bind:value={formDescription} />
					</div>
					{#if meta && meta.languages.length > 0}
						<div class="grid gap-1.5">
							<Label>Language</Label>
							{@render languageSelect()}
						</div>
					{/if}
				</div>
			{:else if dialogMode === 'password' && dialogTarget}
				<Dialog.Header>
					<Dialog.Title>Reset password of {dialogTarget.login}</Dialog.Title>
					<Dialog.Description>The user is logged out and has to log in with the new password.</Dialog.Description>
				</Dialog.Header>
				<div class="grid gap-3 py-2">
					{@render passwordFields(false)}
				</div>
			{:else if dialogMode === 'ownpassword'}
				<Dialog.Header>
					<Dialog.Title>Change my password</Dialog.Title>
					<Dialog.Description>Your other sessions are logged out. This session stays open.</Dialog.Description>
				</Dialog.Header>
				<div class="grid gap-3 py-2">
					{@render passwordFields(true)}
				</div>
			{:else if dialogMode === 'delete' && dialogTarget}
				<Dialog.Header>
					<Dialog.Title>Delete {dialogTarget.login}?</Dialog.Title>
					<Dialog.Description>The user can not log in anymore and is logged out now. Builds and logs keep the user's name.</Dialog.Description>
				</Dialog.Header>
			{:else if dialogMode === 'sessions' && dialogTarget}
				<Dialog.Header>
					<Dialog.Title>Sessions of {dialogTarget.login}</Dialog.Title>
					<Dialog.Description>Ending the sessions logs the user out on all devices.</Dialog.Description>
				</Dialog.Header>
				<div class="grid gap-1 py-2 text-sm">
					{#each sessions as session, index (index)}
						<div class="flex justify-between border-b py-1 last:border-b-0">
							<span>Session {index + 1}</span>
							<span class="text-muted-foreground text-xs">last active {formatActivity(session.lastActivity)}</span>
						</div>
					{:else}
						<span class="text-muted-foreground">No open sessions.</span>
					{/each}
				</div>
			{/if}

			{#if dialogError}
				<div class="text-sm text-destructive flex items-start gap-2">
					<CircleAlert class="h-4 w-4 mt-0.5 shrink-0" />
					<span>{dialogError}</span>
				</div>
			{/if}

			<Dialog.Footer class="mt-2">
				<Button variant="outline" onclick={closeDialog} disabled={busy}>Cancel</Button>
				{#if dialogMode === 'create'}
					<Button onclick={submitCreate} disabled={busy}>Create user</Button>
				{:else if dialogMode === 'edit'}
					<Button onclick={submitEdit} disabled={busy}>Save</Button>
				{:else if dialogMode === 'ownprofile'}
					<Button onclick={submitOwnProfile} disabled={busy}>Save</Button>
				{:else if dialogMode === 'password'}
					<Button onclick={submitPasswordReset} disabled={busy}>Reset password</Button>
				{:else if dialogMode === 'ownpassword'}
					<Button onclick={submitOwnPassword} disabled={busy}>Change password</Button>
				{:else if dialogMode === 'delete' && dialogTarget}
					{@const target = dialogTarget}
					<Button variant="destructive" onclick={() => submitDelete(target)} disabled={busy}>Delete user</Button>
				{:else if dialogMode === 'sessions' && sessions.length > 0}
					<Button variant="destructive" onclick={submitRevoke} disabled={busy}>End all sessions</Button>
				{/if}
			</Dialog.Footer>
		</Dialog.Content>
	</Dialog.Root>
{/if}
