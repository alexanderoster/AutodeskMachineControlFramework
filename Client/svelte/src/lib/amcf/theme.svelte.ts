const NULL_UUID = '00000000-0000-0000-0000-000000000000';

/*
 * Application-wide theme state. It mirrors the `dark` class on the root element, which is set
 * through the theme loader; components read `theme.dark` to re-render on a theme switch.
 */
export const theme = $state({ dark: false });

function isValidUUID (uuid: string | null | undefined): uuid is string {
	return !!uuid && uuid !== NULL_UUID;
}

/*
 * Returns the resource to show for the current theme: the dark variant while dark mode is
 * active and the dark variant exists, otherwise the light one. Returns '' if neither is set.
 */
export function themedImageUUID (lightUUID: string | null | undefined, darkUUID: string | null | undefined): string {
	if (theme.dark && isValidUUID(darkUUID))
		return darkUUID;
	if (isValidUUID(lightUUID))
		return lightUUID;
	return '';
}
