/*++

Copyright (C) 2026 Autodesk Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Autodesk Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

// Color maps after the ParaView presets. Stops are [position (0..1), red, green, blue (0..255)].

const COOLTOWARM_TABLE = [
	[59, 76, 192], [68, 90, 204], [77, 104, 215], [87, 117, 225], [98, 130, 234], [108, 142, 241], [119, 154, 247], [130, 165, 251],
	[141, 176, 254], [152, 185, 255], [163, 194, 255], [174, 201, 253], [184, 208, 249], [194, 213, 244], [204, 217, 238], [213, 219, 230],
	[221, 221, 221], [229, 216, 209], [236, 211, 197], [241, 204, 185], [245, 196, 173], [247, 187, 160], [247, 177, 148], [247, 166, 135],
	[244, 154, 123], [241, 141, 111], [236, 127, 99], [229, 112, 88], [222, 96, 77], [213, 80, 66], [203, 62, 56], [192, 40, 47], [180, 4, 38]
];

const COOLTOWARM_STOPS = COOLTOWARM_TABLE.map ((rgb, index) => [index / (COOLTOWARM_TABLE.length - 1), rgb[0], rgb[1], rgb[2]]);

const fromUnitRGB = (stops) => stops.map ((stop) => [stop[0], Math.round (stop[1] * 255), Math.round (stop[2] * 255), Math.round (stop[3] * 255)]);

export const COLORMAP_PRESETS = [
	{ name: "cooltowarm", caption: "Cool to Warm", stops: COOLTOWARM_STOPS },
	{ name: "warmtocool", caption: "Warm to Cool", stops: COOLTOWARM_STOPS.map ((stop) => [1 - stop[0], stop[1], stop[2], stop[3]]).reverse () },
	{ name: "grayscale", caption: "Grayscale", stops: [[0, 0, 0, 0], [1, 255, 255, 255]] },
	{ name: "rainbowdesaturated", caption: "Rainbow Desaturated", stops: fromUnitRGB ([
		[0.0, 0.278, 0.278, 0.859], [0.143, 0.0, 0.0, 0.361], [0.285, 0.0, 1.0, 1.0], [0.429, 0.0, 0.502, 0.0],
		[0.571, 1.0, 1.0, 0.0], [0.714, 1.0, 0.380, 0.0], [0.857, 0.420, 0.0, 0.0], [1.0, 0.878, 0.302, 0.302]]) },
	{ name: "bluetoredrainbow", caption: "Blue to Red Rainbow", stops: fromUnitRGB ([
		[0.0, 0.0, 0.0, 1.0], [0.25, 0.0, 1.0, 1.0], [0.5, 0.0, 1.0, 0.0], [0.75, 1.0, 1.0, 0.0], [1.0, 1.0, 0.0, 0.0]]) },
	{ name: "jet", caption: "Jet", stops: fromUnitRGB ([
		[0.0, 0.0, 0.0, 0.5625], [0.111111, 0.0, 0.0, 1.0], [0.365079, 0.0, 1.0, 1.0], [0.492063, 0.5, 1.0, 0.5],
		[0.619048, 1.0, 1.0, 0.0], [0.873016, 1.0, 0.0, 0.0], [1.0, 0.5, 0.0, 0.0]]) }
];

export const DEFAULT_COLORMAP = "cooltowarm";

export function findColorMap (name)
{
	return COLORMAP_PRESETS.find ((preset) => preset.name === name) || COLORMAP_PRESETS[0];
}

// Evaluates a color map. value is clamped to 0..1; returns the color as 0xRRGGBB.
export class ColorMapSampler {

	constructor (name, inverted)
	{
		this.stops = findColorMap (name).stops;
		this.inverted = inverted === true;
		// Stop positions in value space, ascending.
		const positions = this.stops.map ((stop) => this.inverted ? (1 - stop[0]) : stop[0]);
		this.positions = positions.sort ((a, b) => a - b);
	}

	colorAt (value)
	{
		let t = Number.isFinite (value) ? Math.min (Math.max (value, 0.0), 1.0) : 0.0;
		if (this.inverted)
			t = 1.0 - t;

		const stops = this.stops;
		let upper = 1;
		while ((upper < stops.length - 1) && (stops[upper][0] < t))
			upper++;
		const a = stops[upper - 1];
		const b = stops[upper];
		const span = b[0] - a[0];
		const f = (span > 0) ? Math.min (Math.max ((t - a[0]) / span, 0.0), 1.0) : 0.0;

		const red = Math.round (a[1] + (b[1] - a[1]) * f);
		const green = Math.round (a[2] + (b[2] - a[2]) * f);
		const blue = Math.round (a[3] + (b[3] - a[3]) * f);
		return (red << 16) | (green << 8) | blue;
	}

}

// CSS linear-gradient of a color map, from value 0 (left) to 1 (right).
export function colorMapCSSGradient (name, inverted, direction = "to right")
{
	const sampler = new ColorMapSampler (name, inverted);
	const entries = sampler.positions.map ((position) => {
		const color = sampler.colorAt (position);
		return "#" + color.toString (16).padStart (6, "0") + " " + (position * 100).toFixed (2) + "%";
	});
	return "linear-gradient(" + direction + ", " + entries.join (", ") + ")";
}
