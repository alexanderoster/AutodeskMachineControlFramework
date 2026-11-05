// Service layer for the AMCF data capture REST API (/api/datatable/*).
//
// All requests go through the shared AMCApplication axios wrapper so that the
// bearer token and base URL handling stay consistent with the rest of the client.
//
// A capture can hold tens of GB. The server therefore never returns a capture as
// a whole: a request names a row range, and a chart asks for a downsampled
// envelope whose size follows the pixel width of the chart.

export interface DataCaptureColumn {
	name: string;
	description: string;
	type: string;
	quantization: number;
	hasRange: boolean;
	min: number;
	max: number;
}

export interface DataCaptureSegment {
	index: number;
	firstRow: number;
	rowCount: number;
	size: number;
	available: boolean;
}

export interface DataCaptureInfo {
	uuid: string;
	identifier: string;
	name: string;
	status: string;
	timestamp: string;
	rowCount: number;
	firstAvailableRow: number;
	storedSize: number;
	// The server rejects data requests that return more values than this.
	maxValueCount: number;
	maxTargetPoints: number;
	columns: DataCaptureColumn[];
	segments: DataCaptureSegment[];
}

export interface DataCaptureBucket {
	row: number;
	min: number;
	max: number;
}

export interface DataCaptureSeries {
	name: string;
	available: boolean;
	buckets: DataCaptureBucket[];
}

export interface DataCaptureEnvelope {
	fromRow: number;
	rowCount: number;
	series: DataCaptureSeries[];
}

export interface DataCaptureRowColumn {
	name: string;
	type: string;
	values: (number | boolean | null)[];
}

export interface DataCaptureRows {
	fromRow: number;
	rowCount: number;
	columns: DataCaptureRowColumn[];
}

function columnList (columns: string[]): string {
	return columns.map((name) => encodeURIComponent(name)).join(',');
}

// JSON has no NaN, the server sends null for values that are not finite.
function toNumber (value: unknown): number {
	return (value === null || value === undefined) ? NaN : Number(value);
}

// Retrieves schema, row counts, segments and the value range of every column.
// Only the manifest and the segment footers are read on the server.
export async function getDataCaptureInfo (app: any, captureUUID: string): Promise<DataCaptureInfo> {
	const response = await app.axiosGetRequest(`/datatable/${encodeURIComponent(captureUUID)}/info`);
	const data = response.data || {};

	return {
		uuid: String(data.uuid || captureUUID),
		identifier: String(data.identifier || ''),
		name: String(data.name || ''),
		status: String(data.status || 'unknown'),
		timestamp: String(data.timestamp || ''),
		rowCount: Number(data.rowcount || 0),
		firstAvailableRow: Number(data.firstavailablerow || 0),
		storedSize: Number(data.storedsize || 0),
		maxValueCount: Number(data.maxvaluecount || 0),
		maxTargetPoints: Number(data.maxtargetpoints || 0),
		columns: ((data.columns as any[]) || []).map((entry) => ({
			name: String(entry.name),
			description: String(entry.description || ''),
			type: String(entry.type),
			quantization: Number(entry.quantization || 0),
			hasRange: entry.hasrange === true,
			min: Number(entry.min),
			max: Number(entry.max)
		})),
		segments: ((data.segments as any[]) || []).map((entry) => ({
			index: Number(entry.index),
			firstRow: Number(entry.firstrow),
			rowCount: Number(entry.rowcount),
			size: Number(entry.size),
			available: entry.available === true
		}))
	};
}

// Retrieves the min/max envelope of one or more columns over a row range.
// `targetPoints` should be roughly the pixel width of the chart: the response is
// bounded by it and not by the length of the capture, and single-sample spikes
// stay visible because every bucket reports its extremes.
export async function getDataCaptureEnvelope (
	app: any,
	captureUUID: string,
	columns: string[],
	fromRow: number,
	rowCount: number,
	targetPoints: number
): Promise<DataCaptureEnvelope> {
	if (columns.length === 0 || rowCount <= 0)
		return { fromRow, rowCount, series: [] };

	const query =
		`/datatable/${encodeURIComponent(captureUUID)}?format=json&downsample=minmax` +
		`&columns=${columnList(columns)}` +
		`&fromrow=${Math.floor(fromRow)}` +
		`&rowcount=${Math.floor(rowCount)}` +
		`&targetpoints=${Math.floor(targetPoints)}`;

	const response = await app.axiosGetRequest(query);
	const data = response.data || {};

	return {
		fromRow: Number(data.fromrow ?? fromRow),
		rowCount: Number(data.rowcount ?? rowCount),
		series: ((data.series as any[]) || []).map((entry) => ({
			name: String(entry.name),
			available: entry.available !== false,
			buckets: ((entry.buckets as any[]) || []).map((bucket) => ({
				row: Number(bucket.row),
				min: toNumber(bucket.min),
				max: toNumber(bucket.max)
			}))
		}))
	};
}

// Retrieves a window of rows with their exact values, for a table view.
export async function getDataCaptureRows (
	app: any,
	captureUUID: string,
	columns: string[],
	fromRow: number,
	rowCount: number
): Promise<DataCaptureRows> {
	if (columns.length === 0 || rowCount <= 0)
		return { fromRow, rowCount: 0, columns: [] };

	const query =
		`/datatable/${encodeURIComponent(captureUUID)}?format=json` +
		`&columns=${columnList(columns)}` +
		`&fromrow=${Math.floor(fromRow)}` +
		`&rowcount=${Math.floor(rowCount)}`;

	const response = await app.axiosGetRequest(query);
	const data = response.data || {};

	return {
		fromRow: Number(data.fromrow ?? fromRow),
		rowCount: Number(data.rowcount ?? rowCount),
		columns: ((data.columns as any[]) || []).map((entry) => ({
			name: String(entry.name),
			type: String(entry.type),
			values: (entry.values as (number | boolean | null)[]) || []
		}))
	};
}

// Retrieves a row range as CSV text. The range must stay within the value limit
// that the info document reports (maxValueCount).
export async function getDataCaptureCSV (
	app: any,
	captureUUID: string,
	columns: string[],
	fromRow: number,
	rowCount: number
): Promise<string> {
	const query =
		`/datatable/${encodeURIComponent(captureUUID)}?format=csv` +
		`&columns=${columnList(columns)}` +
		`&fromrow=${Math.floor(fromRow)}` +
		`&rowcount=${Math.floor(rowCount)}`;

	const response = await app.axiosGetRequest(query, { responseType: 'text' });
	return String(response.data ?? '');
}

// Requests a download ticket for one segment of the capture and returns its uuid.
// The segment is the native data table stream and is downloaded through the
// regular ticket flow of the download endpoint.
export async function requestSegmentDownloadTicket (app: any, captureUUID: string, segmentIndex: number): Promise<string> {
	const response = await app.axiosGetRequest(`/datatable/${encodeURIComponent(captureUUID)}/segment/${Math.floor(segmentIndex)}`);
	const data = response.data || {};
	return String(data.ticketuuid || '');
}
