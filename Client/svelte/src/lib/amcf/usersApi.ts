// Service layer for the AMCF user management REST API (/api/users/*).
//
// All requests go through the shared AMCApplication axios wrapper so that the bearer
// token and base URL handling stay consistent with the rest of the client. Passwords
// never leave the browser in clear text: only a random salt and sha256(salt + password)
// are sent, which is the format the login handshake expects.

import { sha256Hex } from '@core/common/AMCCrypto.js';

export type UserCapability =
	| 'viewusers'
	| 'createuser'
	| 'editdetails'
	| 'assignrole'
	| 'resetpassword'
	| 'deleteuser'
	| 'managesessions'
	| 'changeownpassword'
	| 'editownprofile';

export interface UserRole {
	identifier: string;
	displayName: string;
	description: string;
	permissions: string[];
	assignable: boolean;
	isDefault: boolean;
	deprecated: boolean;
}

export interface UserPermission {
	identifier: string;
	displayName: string;
	description: string;
}

export interface UserLanguage {
	identifier: string;
	displayName: string;
}

export interface PasswordPolicy {
	minimumLength: number;
	requireDigit: boolean;
	requireLetter: boolean;
}

export interface UsersMeta {
	capabilities: Record<UserCapability, boolean>;
	roles: UserRole[];
	permissions: UserPermission[];
	languages: UserLanguage[];
	passwordPolicy: PasswordPolicy;
	self: { uuid: string; login: string; role: string };
	// True if the account test/test from older installations still exists.
	defaultTestUser: boolean;
}

export interface FirstRunInfo {
	firstRun: boolean;
	available: boolean;
	setupCodeRequired: boolean;
	welcomeTitle: string;
	welcomeText: string;
	gitHash: string;
	nextSteps: { text: string; page: string }[];
	adminRole: { identifier: string; displayName: string; description: string; permissions: UserPermission[] } | null;
	languages: UserLanguage[];
	passwordPolicy: PasswordPolicy;
}

export interface ManagedUser {
	uuid: string;
	login: string;
	description: string;
	role: string;
	roleName: string;
	language: string;
	isSelf: boolean;
	manageable: boolean;
	online: boolean;
	sessionCount: number;
	lastActivity: number; // microseconds since 1970, 0 if no session
}

export interface UserList {
	users: ManagedUser[];
	serverTime: number;
}

export interface UserSession {
	lastActivity: number;
	isSelf: boolean;
}

export interface UserUpdate {
	description?: string;
	language?: string;
	role?: string;
}

const ALL_CAPABILITIES: UserCapability[] = [
	'viewusers',
	'createuser',
	'editdetails',
	'assignrole',
	'resetpassword',
	'deleteuser',
	'managesessions',
	'changeownpassword',
	'editownprofile'
];

// Minimum length the core client accepts for a password, see AMCCommon.minimumPasswordLength.
const ABSOLUTE_MINIMUM_PASSWORD_LENGTH = 5;

function mapUser(entry: any): ManagedUser {
	return {
		uuid: String(entry.uuid || ''),
		login: String(entry.login || ''),
		description: String(entry.description || ''),
		role: String(entry.role || ''),
		roleName: String(entry.rolename || entry.role || ''),
		language: String(entry.language || ''),
		isSelf: entry.isself === true,
		manageable: entry.manageable === true,
		online: entry.online === true,
		sessionCount: Number(entry.sessioncount || 0),
		lastActivity: Number(entry.lastactivity || 0)
	};
}

function userPath(uuid: string): string {
	return '/users/' + encodeURIComponent(uuid);
}

function mapLanguages(entries: any): UserLanguage[] {
	return ((entries as any[]) || []).map((entry) => ({
		identifier: String(entry.identifier),
		displayName: String(entry.displayname || entry.identifier)
	}));
}

function mapPasswordPolicy(policy: any): PasswordPolicy {
	const source = policy || {};
	return {
		minimumLength: Math.max(ABSOLUTE_MINIMUM_PASSWORD_LENGTH, Number(source.minimumlength || 0)),
		requireDigit: source.requiredigit === true,
		requireLetter: source.requireletter === true
	};
}

export async function getUsersMeta(app: any): Promise<UsersMeta> {
	const response = await app.axiosGetRequest('/users/meta');
	const data = response.data || {};
	const rawCapabilities = data.capabilities || {};

	const capabilities = {} as Record<UserCapability, boolean>;
	for (const capability of ALL_CAPABILITIES) capabilities[capability] = rawCapabilities[capability] === true;

	const self = data.self || {};

	return {
		capabilities,
		roles: ((data.roles as any[]) || []).map((entry) => ({
			identifier: String(entry.identifier),
			displayName: String(entry.displayname || entry.identifier),
			description: String(entry.description || ''),
			permissions: ((entry.permissions as any[]) || []).map((permission) => String(permission)),
			assignable: entry.assignable === true,
			isDefault: entry.isdefault === true,
			deprecated: entry.deprecated === true
		})),
		permissions: ((data.permissions as any[]) || []).map((entry) => ({
			identifier: String(entry.identifier),
			displayName: String(entry.displayname || entry.identifier),
			description: String(entry.description || '')
		})),
		languages: mapLanguages(data.languages),
		passwordPolicy: mapPasswordPolicy(data.passwordpolicy),
		self: {
			uuid: String(self.uuid || ''),
			login: String(self.login || ''),
			role: String(self.role || '')
		},
		defaultTestUser: data.defaulttestuser === true
	};
}

export async function getFirstRunInfo(app: any): Promise<FirstRunInfo> {
	const response = await app.axiosGetRequest('/firstrun');
	const data = response.data || {};
	const role = data.adminrole;

	return {
		firstRun: data.firstrun === true,
		available: data.available === true,
		setupCodeRequired: data.setupcoderequired !== false,
		welcomeTitle: String(data.welcometitle || ''),
		welcomeText: String(data.welcometext || ''),
		gitHash: String(data.githash || ''),
		nextSteps: ((data.nextsteps as any[]) || []).map((entry) => ({
			text: String(entry.text || ''),
			page: String(entry.page || '')
		})),
		adminRole: role
			? {
					identifier: String(role.identifier),
					displayName: String(role.displayname || role.identifier),
					description: String(role.description || ''),
					permissions: ((role.permissions as any[]) || []).map((entry) => ({
						identifier: String(entry.identifier),
						displayName: String(entry.displayname || entry.identifier),
						description: ''
					}))
				}
			: null,
		languages: mapLanguages(data.languages),
		passwordPolicy: mapPasswordPolicy(data.passwordpolicy)
	};
}

export async function createFirstAdministrator(
	app: any,
	account: { login: string; description: string; language: string; password: string; setupCode: string }
): Promise<string> {
	const { salt, hash } = await hashPassword(account.password);
	const response = await app.axiosPostRequest('/firstrun/admin', {
		setupcode: account.setupCode,
		login: account.login,
		description: account.description,
		language: account.language,
		salt,
		passwordhash: hash
	});
	return String((response.data && response.data.useruuid) || '');
}

export function errorCodeOf(err: any): number {
	return Number(err && err.response && err.response.data && err.response.data.errorcode);
}

export async function listUsers(app: any): Promise<UserList> {
	const response = await app.axiosGetRequest('/users');
	const data = response.data || {};
	return {
		users: ((data.users as any[]) || []).map(mapUser),
		serverTime: Number(data.servertime || 0)
	};
}

export async function getOwnUser(app: any): Promise<ManagedUser> {
	const response = await app.axiosGetRequest('/users/me');
	return mapUser((response.data && response.data.user) || {});
}

export async function getUserSessions(app: any, uuid: string): Promise<UserSession[]> {
	const response = await app.axiosGetRequest(userPath(uuid) + '/sessions');
	return (((response.data && response.data.sessions) as any[]) || []).map((entry) => ({
		lastActivity: Number(entry.lastactivity || 0),
		isSelf: entry.isself === true
	}));
}

// Returns a list of problems; an empty list means the password satisfies the policy.
export function checkPasswordPolicy(password: string, policy: PasswordPolicy): string[] {
	const problems: string[] = [];
	if (password.length < policy.minimumLength)
		problems.push(`at least ${policy.minimumLength} characters`);
	if (policy.requireDigit && !/[0-9]/.test(password)) problems.push('at least one digit');
	if (policy.requireLetter && !/[A-Za-z]/.test(password)) problems.push('at least one letter');
	if (password !== password.trim()) problems.push('no spaces at the beginning or end');
	return problems;
}

async function hashPassword(password: string): Promise<{ salt: string; hash: string }> {
	const randomBytes = new Uint8Array(32);
	crypto.getRandomValues(randomBytes);
	let randomString = '';
	for (const value of randomBytes) randomString += value.toString(16).padStart(2, '0');

	const salt = await sha256Hex(randomString);
	const hash = await sha256Hex(salt + password);
	return { salt, hash };
}

export async function createUser(
	app: any,
	login: string,
	description: string,
	role: string,
	language: string,
	password: string
): Promise<string> {
	const { salt, hash } = await hashPassword(password);
	const response = await app.axiosPostRequest('/users', {
		login,
		description,
		role,
		language,
		salt,
		passwordhash: hash
	});
	return String((response.data && response.data.uuid) || '');
}

export async function updateUser(app: any, uuid: string, update: UserUpdate): Promise<void> {
	await app.axiosPostRequest(userPath(uuid), update);
}

export async function resetUserPassword(app: any, uuid: string, password: string): Promise<void> {
	const { salt, hash } = await hashPassword(password);
	await app.axiosPostRequest(userPath(uuid) + '/password', { salt, passwordhash: hash });
}

export async function deleteUser(app: any, uuid: string): Promise<void> {
	await app.axiosPostRequest(userPath(uuid) + '/delete', {});
}

export async function revokeUserSessions(app: any, uuid: string): Promise<number> {
	const response = await app.axiosPostRequest(userPath(uuid) + '/sessions/revoke', {});
	return Number((response.data && response.data.revoked) || 0);
}

export async function updateOwnProfile(app: any, update: { description?: string; language?: string }): Promise<void> {
	await app.axiosPostRequest('/users/me', update);
}

// Proves knowledge of the current password without sending it or its stored hash:
// proof = sha256(nonce + sha256(salt + currentPassword)).
export async function changeOwnPassword(app: any, currentPassword: string, newPassword: string): Promise<void> {
	const challengeResponse = await app.axiosPostRequest('/users/me/passwordchallenge', {});
	const challenge = challengeResponse.data || {};

	const storedHash = await sha256Hex(String(challenge.salt || '') + currentPassword);
	const proof = await sha256Hex(String(challenge.nonce || '') + storedHash);
	const { salt, hash } = await hashPassword(newPassword);

	await app.axiosPostRequest('/users/me/password', {
		challengeuuid: challenge.challengeuuid,
		proof,
		salt,
		passwordhash: hash
	});
}

// LibMC error codes (ACT/LibMC.xml) mapped to readable messages.
const ERROR_MESSAGES: Record<number, string> = {
	285: 'The login is not valid. Use letters, digits and _ - . @ only.',
	301: 'The user does not exist anymore.',
	581: 'The language is not available.',
	756: 'You are not allowed to do this.',
	757: 'You can not assign this role or manage this user.',
	758: 'This is not possible on your own account.',
	759: 'At least one user must keep user management rights.',
	760: 'The current password is not correct.',
	761: 'The request has expired. Please try again.',
	764: 'A user with this login already exists.',
	765: 'The description is not valid.',
	766: 'The setup has already been completed on another computer.',
	767: 'The setup code is not correct. After several wrong codes a new one is shown in the server console.',
	768: 'The administrator role of this machine is not configured correctly.',
	770: 'The setup is not available, because no administrator role is configured.'
};

export const ERROR_FIRSTRUN_ALREADY_COMPLETED = 766;
export const ERROR_INVALID_SETUP_CODE = 767;

// Maps server errors to readable messages; unknown errors fall back to the server message.
export function describeUserError(app: any, err: any): string {
	const errorCode = errorCodeOf(err);
	if (errorCode in ERROR_MESSAGES) return ERROR_MESSAGES[errorCode];
	if (app && typeof app.extractErrorMessage === 'function') return app.extractErrorMessage(err);
	return String(err);
}
