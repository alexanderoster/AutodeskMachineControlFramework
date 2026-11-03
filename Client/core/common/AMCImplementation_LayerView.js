/*++

Copyright (C) 2020 Autodesk Inc.

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

const LAYERVIEW_MINSCALING = 0.4;
const LAYERVIEW_MAXSCALING = 4000.0;
const LAYERVIEW_MINVELOCITYRANGE = 1.0;

// Default scatter point size calibration (pixels = base + factor * pixels per mm, clamped).
const LAYERVIEW_POINTSIZE_BASE = 2.0;
const LAYERVIEW_POINTSIZE_FACTOR = 0.08;
const LAYERVIEW_POINTSIZE_MIN = 2.0;
const LAYERVIEW_POINTSIZE_MAX = 16.0;

// Fallback marking speed for segments without a laser speed, in mm/s.
const LAYERVIEW_DEFAULTMARKSPEED = 1000.0;

// Aligning toolpath vectors with a recording: a laser-on run belongs to a vector if its first
// sample lies within the tolerance (mm) of one of the vector's lines. Runs are searched this
// far ahead, and only this many lines of long polylines are tested.
const LAYERVIEW_TIMING_MATCHTOLERANCE = 0.05;
const LAYERVIEW_TIMING_RUNLOOKAHEAD = 200;
const LAYERVIEW_TIMING_MAXGROUPLINES = 64;

// Segments of parts that have been disabled are still drawn, but in this neutral gray.
const LAYERVIEW_DISABLEDPARTCOLOR = 0xB4B4B4;

// Hovered line overlay: color, arrow head at the end point (minimum length in CSS pixels and
// in line thicknesses, wing thickness relative to the line, half opening angle) and the
// support points of nonlinear power ramps (diameter in CSS pixels).
const LAYERVIEW_HIGHLIGHTCOLOR = 0x00e5ff;
const LAYERVIEW_HIGHLIGHTARROWPIXELS = 10.0;
const LAYERVIEW_HIGHLIGHTARROWTHICKNESSFACTOR = 2.0;
const LAYERVIEW_HIGHLIGHTARROWWINGFACTOR = 0.6;
const LAYERVIEW_HIGHLIGHTARROWANGLE = 28.0 * Math.PI / 180.0;
const LAYERVIEW_SUPPORTPOINTPIXELS = 9.0;
const LAYERVIEW_SUPPORTPOINTCOLOR = 0xffb300;
const LAYERVIEW_SUPPORTENDPOINTCOLOR = 0xffffff;

// Grid level-of-detail switch band. The grid geometry is self-similar every 5
// subdivisions, so the lower/upper bounds MUST keep a 5x ratio for the wrap to
// stay seamless (LOWER * GRID_LOD_RECURSION === UPPER). Lowering the band
// compared to the previous 0.5..2.5 keeps the on-screen grid finer and avoids
// the coarse, "clunky" cells that appeared at the top of the old band right
// after a zoom step crossed a threshold.
const LAYERVIEW_GRIDLOD_LOWER = 0.25;
const LAYERVIEW_GRIDLOD_UPPER = 1.25;
const LAYERVIEW_GRIDLOD_RECURSION = 5.0;

// The grid mesh is (re)generated only on viewport resize, but at render time it
// is scaled by gridScale, which can drop as low as LAYERVIEW_GRIDLOD_LOWER. The
// mesh must therefore span (viewport / LOWER) model units so it still fills the
// screen at the coarsest LOD. Deriving it from LOWER keeps the two in sync if
// the LOD band is ever retuned (e.g. 1 / 0.25 => a 4x-viewport mesh).
const LAYERVIEW_GRIDMESH_VIEWPORTFACTOR = Math.ceil (1.0 / LAYERVIEW_GRIDLOD_LOWER);

class LayerViewImpl {

    constructor(glInstance) {

        this.glInstance = glInstance;

        this.currentSize = {
            gridWidth: 0,
            gridHeight: 0,
            viewPortWidth: 0,
            viewPortHeight: 0
        }
        this.transform = {
            x: 0,
            y: 0,
            scaling: 1.0
        }
        this.origin = {
            x: 0,
            y: 0
        }
        this.transformAngleDegrees = 0.0;
        this.rotationCenter = {
            x: 0,
            y: 0
        };
        this.translation = {
            x: 0,
            y: 0
        };
		
		this.lineScaleLevel = 0;
		this.renderNeedsUpdate = true;

		this.layerSegmentsArray = null;
		this.layerPartNames = new Map ();
		this.layerDisabledParts = new Set ();
		this.partBoundsCache = null;
		this.partBoundsCacheKey = "";

		// Invoked after every pan/zoom/placement change so screen-space overlays can follow.
		this.onTransformChanged = null;
		
		this.layerPointsMode = "unicolor";
		this.layerPointsArray = null;
		this.layerPointsColorArray = null;
		this.layerPointsVelocities = [];
		this.layerPointsMaxVelocity = LAYERVIEW_MINVELOCITYRANGE;
		this.layerPointsMinVelocity = LAYERVIEW_MINVELOCITYRANGE;
		this.pointSizeBase = LAYERVIEW_POINTSIZE_BASE;
		this.pointSizeFactor = LAYERVIEW_POINTSIZE_FACTOR;
		this.pointSizeMin = LAYERVIEW_POINTSIZE_MIN;
		this.pointSizeMax = LAYERVIEW_POINTSIZE_MAX;
		
        this.linesCoordinates = [];
		this.segmentProperties = [];
		this.lineGroupIDs = null;
		this.lineDurations = null;
		// Modulated laser power (W) at the start and end of every line, NaN for the nominal power,
		// and the inner support points {t, power} of nonlinear ramps by line index.
		this.linePowers = [];
		this.lineSupportPoints = new Map ();
		this.highlightLineIndex = -1;
		this.highlightScaling = 0.0;
		this.highlightSupportPoints = null;

		// Start time (ms) of every toolpath line on the layer timeline, see updateTiming.
		this.lineStartTimes = null;
		this.timelineDuration = 0.0;
		this.timingSource = "none";
		this.timingMatchRatio = 0.0;
		// Fraction (0..1) of the layer timeline that is drawn; 1 shows everything.
		this.timeCapFraction = 1.0;
		this.visibleLineCount = 0;
		this.visiblePointCount = 0;
		
		this.toolpathVisible = true;
		this.pointsVisible = true;
		this.showLaserOffPoints = false;
		this.filteredToOriginalIndexMap = null;

		this.gridColor = undefined;
        
		this.updateTransform ();

    }

    applyColors(colorSet) {
        if (!colorSet) return;
        if (colorSet.background && this.glInstance)
            this.glInstance.setBackgroundColor(colorSet.background);
        if (colorSet.grid !== undefined)
            this.gridColor = colorSet.grid;
        this.rebuildGrid();
        this.RenderScene(true);
    }

    rebuildGrid() {
        if (!this.glInstance) return;
        if (this.currentSize.gridWidth <= 0 || this.currentSize.gridHeight <= 0) return;
        var gridObject = this.glInstance.scene.getObjectByName("grid");
        if (gridObject)
            this.glInstance.scene.remove(gridObject);
        this.glInstance.add2DGridGeometry("grid", this.currentSize.gridWidth, this.currentSize.gridHeight, 50, 5, this.gridColor);
        this.updateTransform();
        this.renderNeedsUpdate = true;
    }

    updateSize(width, height) {
        if ((width > 0) && (height > 0)) {

            this.currentSize.viewPortWidth = width;
            this.currentSize.viewPortHeight = height;

            if (this.glInstance) {
                this.glInstance.resizeTo(width, height);
                this.glInstance.setup2DView(width, height, 0.1, 100);
            }

            var newWidth = width * LAYERVIEW_GRIDMESH_VIEWPORTFACTOR + 50;
            var newHeight = height * LAYERVIEW_GRIDMESH_VIEWPORTFACTOR + 50;

            if ((this.currentSize.gridWidth < newWidth) || (this.currentSize.gridHeight < newHeight)) {
                this.currentSize.gridWidth = newWidth;
                this.currentSize.gridHeight = newHeight;

                this.createGrid(newWidth, newHeight);

            }

        }

		this.renderNeedsUpdate = true;
    }

    createGrid(width, height) {
        if (!this.glInstance)
            return;

        var gridObject = this.glInstance.scene.getObjectByName("grid");
        if (gridObject)
            this.glInstance.scene.remove(gridObject);

        this.glInstance.add2DGridGeometry("grid", width, height, 50, 5, this.gridColor);
		this.updateTransform ();
		
		this.renderNeedsUpdate = true;
		

    }

    transformToolpathPoint(x, y) {
        const angleInRadians = this.transformAngleDegrees * Math.PI / 180.0;
        const cosine = Math.cos(angleInRadians);
        const sine = Math.sin(angleInRadians);
        const relativeX = x - this.rotationCenter.x;
        const relativeY = y - this.rotationCenter.y;

        return {
            x: relativeX * cosine - relativeY * sine + this.rotationCenter.x + this.translation.x,
            y: relativeX * sine + relativeY * cosine + this.rotationCenter.y + this.translation.y
        };
    }

    updateTransform() {

        if (!this.glInstance)
            return;

        const placementOffset = this.transformToolpathPoint(0, 0);
        const toolpathPositionX = this.transform.x + (this.origin.x + placementOffset.x) * this.transform.scaling;
        const toolpathPositionY = this.transform.y - (this.origin.y + placementOffset.y) * this.transform.scaling;

        var gridgeometry = this.glInstance.findElement("grid");
        if (gridgeometry) {
            var gridScale = this.transform.scaling;
			var lineScaleLevel = 1;

            if (gridScale < LAYERVIEW_GRIDLOD_LOWER) {
                gridScale = gridScale * LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.8;
            }

            if (gridScale > LAYERVIEW_GRIDLOD_UPPER) {
                gridScale = gridScale / LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.4;
            }

            if (gridScale > LAYERVIEW_GRIDLOD_UPPER) {
                gridScale = gridScale / LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.25;
            }

            if (gridScale > LAYERVIEW_GRIDLOD_UPPER) {
                gridScale = gridScale / LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.125;
            }

            if (gridScale > LAYERVIEW_GRIDLOD_UPPER) {
                gridScale = gridScale / LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.06;
            }

            if (gridScale > LAYERVIEW_GRIDLOD_UPPER) {
                gridScale = gridScale / LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.02;
            }
						
            if (gridScale > LAYERVIEW_GRIDLOD_UPPER) {
                gridScale = gridScale / LAYERVIEW_GRIDLOD_RECURSION;
				lineScaleLevel = 0.01;
            }

			this.updateLineScaleLevel (lineScaleLevel);

            var fullGridSize = gridScale * 25.0 * 5;
            var gridTranslationX = this.transform.x - Math.ceil((this.transform.x / fullGridSize)) * fullGridSize;
            var gridTranslationY = this.transform.y - Math.ceil((this.transform.y / fullGridSize)) * fullGridSize;

            gridgeometry.setPositionXY(gridTranslationX, gridTranslationY);
            gridgeometry.setScaleXY(gridScale, gridScale);
        }

        var layerlinesgeometry = this.glInstance.findElement("layerdata_lines");
        if (layerlinesgeometry) {
            layerlinesgeometry.setPositionXY(toolpathPositionX, toolpathPositionY);
            layerlinesgeometry.setScaleXY(this.transform.scaling,  - this.transform.scaling);
            // Three.js composes rotation before the screen-space Y inversion from
            // setScaleXY. Negating the angle therefore produces a positive
            // counter-clockwise rotation in machine coordinates.
            layerlinesgeometry.setRotationZ(-this.transformAngleDegrees * Math.PI / 180.0);
        }

        // The arrow head is sized in screen pixels, so rebuild the overlay when the zoom changes.
        if ((this.highlightLineIndex >= 0) && (this.highlightScaling !== this.transform.scaling))
            this.buildHighlightGeometry ();
        this.positionHighlightGeometry ();

        var layerpointsgeometry = this.glInstance.findElement("layerdata_points");
        if (layerpointsgeometry) {
            if (typeof layerpointsgeometry.setPointPixelSize === "function")
                layerpointsgeometry.setPointPixelSize(this.computePointPixelSize() * this.glInstance.getPixelRatio());
            layerpointsgeometry.setPositionXY(toolpathPositionX, toolpathPositionY);
            layerpointsgeometry.setScaleXY(this.transform.scaling,  - this.transform.scaling);
            layerpointsgeometry.setRotationZ(-this.transformAngleDegrees * Math.PI / 180.0);
        } 

        var buildplategeometry = this.glInstance.findElement("buildplate");
        if (buildplategeometry) {
            buildplategeometry.setPositionXY(this.transform.x, this.transform.y, buildplategeometry);
            buildplategeometry.setScaleXY(this.transform.scaling,  - this.transform.scaling);
        }
		
		this.renderNeedsUpdate = true;

		if (typeof this.onTransformChanged === "function")
			this.onTransformChanged ();
    }
	
	updateLineScaleLevel (newLineScaleLevel)
	{
		if (newLineScaleLevel != this.lineScaleLevel) {
			
			this.lineScaleLevel = newLineScaleLevel;
			
			this.updateLoadedLayer ();
			this.updateLayerPoints ();

			this.updateTransform();
			this.RenderScene(true);		
			
			
		}
	}
	
	
	



	getPathBoundaries() {
		let layerLines = this.glInstance.findElement("layerdata_lines");
		if (layerLines) {
			const pathBoundaries = layerLines.glelement.geometry.boundingSphere;
			if (!pathBoundaries)
				return null;

			const transformedCenter = this.transformToolpathPoint(pathBoundaries.center.x, pathBoundaries.center.y);

			return {
				center: {
					x: transformedCenter.x,
					y: transformedCenter.y,
					z: pathBoundaries.center.z
				},
				radius: pathBoundaries.radius
			};
		} else {
			return null;
		}
	}


    loadLayer(segmentsArray, partsArray) {
		this.layerSegmentsArray = segmentsArray;
		this.layerPartNames = new Map ();
		this.layerDisabledParts = new Set ();
		if (Array.isArray (partsArray)) {
			for (let part of partsArray) {
				if (part && (typeof part.uuid === "string")) {
					let partUUID = part.uuid.toLowerCase ();
					this.layerPartNames.set (partUUID, (typeof part.name === "string") ? part.name : "");
					if (part.disabled === true)
						this.layerDisabledParts.add (partUUID);
				}
			}
		}
		this.partBoundsCache = null;
		this.updateLoadedLayer ();

		this.updateTransform();
		this.RenderScene(true);		
		
	}
	
	computeVelocities ()
	{
		if (this.layerPointsArray) {
			let pointCount = this.layerPointsArray.length / 2;
			
			if (pointCount > 0) {
				let velocities = [];
				let maxVelocity = LAYERVIEW_MINVELOCITYRANGE;
				let minVelocity = LAYERVIEW_MINVELOCITYRANGE;
				
				for (let pointIndex = 0; pointIndex < pointCount; pointIndex++) {
					let velocity = this.getPointVelocity (pointIndex);					
					if (velocity > 0.0) {						
						if (velocity > LAYERVIEW_MINVELOCITYRANGE) {

							if (maxVelocity < velocity)
								maxVelocity = velocity;
							
							if (minVelocity < LAYERVIEW_MINVELOCITYRANGE) {
								minVelocity = velocity;
							}
							if (minVelocity > velocity) {
								minVelocity = velocity;							
							}
							
						}
													
						velocities.push (velocity);									
					} else {
						velocities.push (0.0);
					}
					

				}
				
				if (maxVelocity < LAYERVIEW_MINVELOCITYRANGE)
					maxVelocity = LAYERVIEW_MINVELOCITYRANGE;
				if (minVelocity < LAYERVIEW_MINVELOCITYRANGE)
					minVelocity = LAYERVIEW_MINVELOCITYRANGE;
				
				this.layerPointsVelocities = velocities;
				this.layerPointsMaxVelocity = maxVelocity;
				this.layerPointsMinVelocity = minVelocity;
			
			}
			
			
		} else {
			this.layerPointsVelocities = null;
			this.layerPointsMaxVelocity = LAYERVIEW_MINVELOCITYRANGE;
			this.layerPointsMinVelocity = LAYERVIEW_MINVELOCITYRANGE;
		}
	}
	
	makeVelocityColors ()
	{
		this.layerPointsColorArray = null;

		if (this.layerPointsVelocities && this.layerPointsArray) {

			const velocityRange = (this.layerPointsMaxVelocity - this.layerPointsMinVelocity);
			if (velocityRange > LAYERVIEW_MINVELOCITYRANGE) {

				let pointCount = this.layerPointsArray.length / 2;
				let colors = [];
				
				for (let pointIndex = 0; pointIndex < pointCount; pointIndex++) {
					let velocity = this.layerPointsVelocities[pointIndex];
					let fraction = (velocity - this.layerPointsMinVelocity) / velocityRange;
					if (fraction >= 0.0) {
						if (fraction > 1.0)
							fraction = 1.0;
					} else {
						fraction = 0.0;
					}
					
					const hue = (1 - fraction) * 240 / 360;
					
					colors.push (this.hslToRgb (hue, 1.0, 0.5));				
					
				}
				
				this.layerPointsColorArray = colors;
			}

		}

	}

	computeChannelColumnRange (pointsChannelName, pointsColumnName)
	{
		if (this[pointsChannelName] && this[pointsChannelName][pointsColumnName]) {
			let dataArray = this[pointsChannelName][pointsColumnName];

			let maxValue = Number.NEGATIVE_INFINITY;
			let minValue = Number.POSITIVE_INFINITY;

			for (let i = 0; i < dataArray.length; i++) {
				let value = dataArray[i];
				if (value > maxValue) {
					maxValue = value;
				}
				if (value < minValue) {
					minValue = value;
				}
			}
			this[pointsChannelName][pointsColumnName].max = maxValue;
			this[pointsChannelName][pointsColumnName].min = minValue;
		}
	}

	makeLaserOnColors ()
	{		
		this.laserOnPointsColorArray = null;

		if (this.laser && this.laser.laseron && this.layerPointsArray) {

			let pointCount = this.layerPointsArray.length / 2;
			let colors = [];
			
			for (let pointIndex = 0; pointIndex < pointCount; pointIndex++) {

				let laseron = this.laser.laseron[pointIndex];

				// laseron = 0 => Blue
				// laseron = 1 => Red
				const hue = laseron * 240 / 360;
				
				colors.push (this.hslToRgb (hue, 1.0, 0.5));				
			}
			
			this.layerPointsColorArray = colors;
		}
		
	}
	
	makeLaserPowerColors ()
	{
		this.layerPointsColorArray = null;

		if (this.laser && this.laser.power && this.layerPointsArray) {
			const powerRange = (this.laser.power.max - this.laser.power.min);
			if (powerRange > 0) {

				let pointCount = this.laser.power.length;
				let colors = [];
				
				for (let pointIndex = 0; pointIndex < pointCount; pointIndex++) {
					let power = this.laser.power[pointIndex];
					let fraction = (power - this.laser.power.min) / powerRange;
					if (fraction >= 0.0) {
						if (fraction > 1.0)
							fraction = 1.0;
					} else {
						fraction = 0.0;
					}
					
					const hue = (fraction) * 240 / 360;
					colors.push (this.hslToRgb (hue, 1.0, 0.5));				
				}
				
				this.layerPointsColorArray = colors;
			}
		}
	}
	
	
	hslToRgb(h, s, l) {
		// Ensure h, s, l are in the range [0, 1]
		h = h % 1; // Wrap around if h is greater than 1
		s = Math.min(Math.max(s, 0), 1);
		l = Math.min(Math.max(l, 0), 1);

		if (s === 0) {
			// Achromatic (gray)
			const value = Math.round(l * 255);
			return [value, value, value];
		}

		const hueToRgb = (p, q, t) => {
			if (t < 0) t += 1;
			if (t > 1) t -= 1;
			if (t < 1 / 6) return p + (q - p) * 6 * t;
			if (t < 1 / 2) return q;
			if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
			return p;
		};

		const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
		const p = 2 * l - q;

		const r = hueToRgb(p, q, h + 1 / 3);
		const g = hueToRgb(p, q, h);
		const b = hueToRgb(p, q, h - 1 / 3);

		return Math.round(r * 255) + Math.round(g * 255) * 256 + Math.round(b * 255) * 65536;
			
	}


	makeTimeColors ()
	{
		this.layerPointsColorArray = null;

		if (this.layerPointsVelocities && this.layerPointsArray) {

			let pointCount = this.layerPointsArray.length / 2;
			let colors = [];
			
			for (let pointIndex = 0; pointIndex < pointCount; pointIndex++) {

				let t = pointIndex / pointCount;
				
				const hue = (1 - t) * 240 / 360;
				
				colors.push (this.hslToRgb (hue, 1.0, 0.5));				
				
			}
			
			this.layerPointsColorArray = colors;

		}

	}

	
    loadPoints(pointsArray) {
		this.layerPointsArray = pointsArray;
		this.layerPointsColorArray = null;
		this.layerPointsVelocities = null;
		this.layerPointsMaxVelocity = LAYERVIEW_MINVELOCITYRANGE;
		this.layerPointsMinVelocity = LAYERVIEW_MINVELOCITYRANGE;
		
		this.computeVelocities ();		
	}

	loadPointsChannelData(pointsChannelName, pointsColumnName, pointsChannelDataArray) {
		
		// Ensure the channel-level object exists
  		if (!Object.prototype.hasOwnProperty.call(this, pointsChannelName)) {
    		this[pointsChannelName] = {};
  		}

  		// Assign the data array to the corresponding column
  		this[pointsChannelName][pointsColumnName] = pointsChannelDataArray;

		this.computeChannelColumnRange (pointsChannelName, pointsColumnName);
	}
	
	clearPoints ()
	{
		this.layerPointsArray = null;
		this.layerPointsColorArray = null;
		this.layerPointsVelocities = null;
		this.layerPointsMaxVelocity = LAYERVIEW_MINVELOCITYRANGE;
		this.layerPointsMinVelocity = LAYERVIEW_MINVELOCITYRANGE;
		
		this.computeVelocities ();		
		this.updateColors ();			
		this.updateLayerPoints ();		
		this.updateTiming ();
	}
	
	clearPointsChannelData (pointsChannelName)
	{
		// Ensure the channel-level object exists
  		if (Object.prototype.hasOwnProperty.call(this, pointsChannelName)) {
    		this[pointsChannelName] = {};
  		}
	
		this.layerPointsColorArray = null;
	}

	updateColors ()
	{
		this.layerPointsColorArray = null;

		if (this.layerPointsMode == "time") {
			this.makeTimeColors ();
		} 

		if (this.layerPointsMode == "velocity") {
			this.makeVelocityColors ();
		} 

		if (this.layerPointsMode == "laseron") {
			this.makeLaserOnColors ();
		}

		if (this.layerPointsMode == "powerramp") {
			this.makeLaserPowerColors ();
		}
	}
	
	setColorMode (newColorMode) {
		this.layerPointsMode = newColorMode;
		this.updateColors ();
		this.updateLayerPoints ();
		this.updateTransform();
		this.RenderScene(true);		
	}
	
	pointDataIsAvailable ()
	{
		if (this.layerPointsArray) {
			return true;
		}
		
		return false;
	}
	
	// Point diameter in CSS pixels for the current zoom (transform.scaling = pixels per mm).
	computePointPixelSize ()
	{
		const scaling = this.transform.scaling > 0 ? this.transform.scaling : 1.0;
		const pixelSize = this.pointSizeBase + this.pointSizeFactor * scaling;
		return Math.min (Math.max (pixelSize, this.pointSizeMin), this.pointSizeMax);
	}

	setPointSizeCalibration (base, factor, minimum, maximum)
	{
		const values = [base, factor, minimum, maximum].map (Number);
		if (values.some ((value) => !Number.isFinite (value)))
			return;

		this.pointSizeBase = values[0];
		this.pointSizeFactor = values[1];
		this.pointSizeMin = Math.max (values[2], 0.5);
		this.pointSizeMax = Math.max (values[3], this.pointSizeMin);
		this.updateTransform ();
		this.RenderScene (true);
	}

	updateLayerPoints () {
		if (!this.glInstance) 
			return;

		this.glInstance.removeElement("layerdata_points");
						
		if (this.layerPointsArray && this.pointsVisible) {
			let pointsToRender = this.layerPointsArray;
			let colorsToRender = this.layerPointsColorArray;
			this.filteredToOriginalIndexMap = null;

			if (!this.showLaserOffPoints && this.laser && this.laser.laseron) {
				let pointCount = this.layerPointsArray.length / 2;
				let filteredCoords = [];
				let filteredColors = colorsToRender ? [] : null;
				let indexMap = [];

				for (let i = 0; i < pointCount; i++) {
					if (this.laser.laseron[i] !== 0) {
						filteredCoords.push(this.layerPointsArray[i * 2], this.layerPointsArray[i * 2 + 1]);
						if (filteredColors && colorsToRender[i] !== undefined) {
							filteredColors.push(colorsToRender[i]);
						}
						indexMap.push(i);
					}
				}
				pointsToRender = new Float32Array(filteredCoords);
				colorsToRender = filteredColors;
				this.filteredToOriginalIndexMap = indexMap;
			}

			const pixelSize = this.computePointPixelSize () * this.glInstance.getPixelRatio ();
			this.glInstance.add2DScreenPointsGeometry("layerdata_points", pointsToRender, 61, pixelSize, 0x00d0ff, colorsToRender);
			this.applyTimeCap ();
			this.updateTransform ();
			this.RenderScene (true);
		}
	}

	// Index of the drawn point closest to the viewport position within pixelTolerance, as an
	// index into the full point array, or -1. Hidden and time-capped points are not picked.
	/**
	 * @param {number} screenX
	 * @param {number} screenY
	 * @param {number} [pixelTolerance]
	 */
	pickPointAtScreenPoint (screenX, screenY, pixelTolerance = 4)
	{
		if (!this.glInstance || !this.layerPointsArray || !this.pointsVisible)
			return -1;

		const pointsElement = this.glInstance.findElement ("layerdata_points");
		if (!pointsElement || (typeof pointsElement.findNearestPoint !== "function"))
			return -1;

		const renderedIndex = pointsElement.findNearestPoint (screenX, screenY, pixelTolerance);
		if (renderedIndex < 0)
			return -1;

		return this.resolvePointIndex (renderedIndex);
	}

	getPointChannelValue (columnName, pointIndex)
	{
		if (this.laser && this.laser[columnName] && (pointIndex >= 0) && (pointIndex < this.laser[columnName].length))
			return this.laser[columnName][pointIndex];
		return null;
	}

	// Measurement values of one recorded point for the hover inspector.
	getPointInfo (pointIndex)
	{
		const position = this.getPointPosition (pointIndex);
		if (!position)
			return null;

		const scannerXColumn = (this.laser && this.laser.targetx) ? "targetx" : "actualx";
		const scannerYColumn = (this.laser && this.laser.targety) ? "targety" : "actualy";

		return {
			index: pointIndex,
			x: position.x,
			y: position.y,
			time: this.getPointChannelValue ("time", pointIndex),
			scannerx: this.getPointChannelValue (scannerXColumn, pointIndex),
			scannery: this.getPointChannelValue (scannerYColumn, pointIndex),
			laseron: this.getPointChannelValue ("laseron", pointIndex),
			power: this.getPointChannelValue ("power", pointIndex),
			velocity: this.getPointVelocity (pointIndex)
		};
	}

	// Number of entries in a sorted array that are smaller than (or equal to, if inclusive) value.
	static countSortedBelow (sortedArray, value, inclusive)
	{
		let low = 0;
		let high = sortedArray.length;
		while (low < high) {
			const middle = (low + high) >> 1;
			const entry = sortedArray[middle];
			if ((entry < value) || (inclusive && (entry === value)))
				low = middle + 1;
			else
				high = middle;
		}
		return low;
	}

	// Assigns a start time to every toolpath line on the layer timeline:
	// - with a recording, the marking vectors are matched to its laser-on runs (alignLinesToRecording);
	// - if nothing matches, the estimated marking time (length / laser speed) is mapped onto the
	//   accumulated laser-on time of the recording;
	// - without a recording, the estimated marking time is used directly (jumps are ignored).
	updateTiming ()
	{
		this.timingMatchRatio = 0.0;
		const lineCount = this.lineDurations ? this.lineDurations.length : 0;
		const lineStartTimes = new Float64Array (lineCount);

		let estimatedTotal = 0.0;
		for (let lineIndex = 0; lineIndex < lineCount; lineIndex++) {
			lineStartTimes[lineIndex] = estimatedTotal;
			estimatedTotal += this.lineDurations[lineIndex];
		}

		const times = this.laser ? this.laser.time : null;
		const laserOn = this.laser ? this.laser.laseron : null;
		const hasRecording = times && (times.length > 1) && this.layerPointsArray;

		if (!hasRecording) {
			this.lineStartTimes = lineStartTimes;
			this.timelineDuration = estimatedTotal;
			this.timingSource = (lineCount > 0) ? "estimate" : "none";
			this.applyTimeCap ();
			return;
		}

		const sampleCount = times.length;
		this.timelineDuration = times[sampleCount - 1];
		this.timingSource = "recording";

		if ((lineCount > 0) && laserOn && (laserOn.length === sampleCount) && (this.layerPointsArray.length >= sampleCount * 2)) {

			const alignedStartTimes = this.alignLinesToRecording (times, laserOn);
			if (alignedStartTimes) {
				this.lineStartTimes = alignedStartTimes;
				this.applyTimeCap ();
				return;
			}

			// Accumulated laser-on time up to each sample.
			const onTime = new Float64Array (sampleCount);
			for (let i = 1; i < sampleCount; i++)
				onTime[i] = onTime[i - 1] + ((laserOn[i - 1] !== 0) ? (times[i] - times[i - 1]) : 0);

			const totalOnTime = onTime[sampleCount - 1];
			if ((totalOnTime > 0) && (estimatedTotal > 0)) {
				for (let lineIndex = 0; lineIndex < lineCount; lineIndex++) {
					const targetOnTime = lineStartTimes[lineIndex] / estimatedTotal * totalOnTime;
					const sampleIndex = Math.min (LayerViewImpl.countSortedBelow (onTime, targetOnTime, false), sampleCount - 1);
					lineStartTimes[lineIndex] = times[sampleIndex];
				}
				this.timingSource = "recording-approximate";
				this.lineStartTimes = lineStartTimes;
				this.applyTimeCap ();
				return;
			}
		}

		// Recording without laser state: stretch the estimate over the recorded duration.
		const stretch = estimatedTotal > 0 ? this.timelineDuration / estimatedTotal : 0;
		for (let lineIndex = 0; lineIndex < lineCount; lineIndex++)
			lineStartTimes[lineIndex] *= stretch;
		this.timingSource = "recording-approximate";
		this.lineStartTimes = lineStartTimes;
		this.applyTimeCap ();
	}

	// Matches the marking vectors (hatch lines and polylines, in toolpath order) to the laser-on
	// runs of the recording. A matched vector starts at the first sample of its run and ends with
	// the last run that still lies on it (vectors can be split into several runs, e.g. at
	// corners). Vectors without a run (too short to be sampled) are interpolated between their
	// matched neighbours by estimated duration. Returns the line start times, or null if no
	// vector could be matched.
	alignLinesToRecording (times, laserOn)
	{
		const lineCount = this.lineDurations.length;
		const sampleCount = times.length;
		const points = this.layerPointsArray;
		const coords = this.linesCoordinates;
		const tolerance = LAYERVIEW_TIMING_MATCHTOLERANCE;
		const toleranceSq = tolerance * tolerance;

		const runStartSamples = [];
		const runEndSamples = [];
		for (let i = 0; i < sampleCount; i++) {
			if ((laserOn[i] !== 0) && ((i === 0) || (laserOn[i - 1] === 0)))
				runStartSamples.push (i);
			if ((laserOn[i] !== 0) && ((i === sampleCount - 1) || (laserOn[i + 1] === 0)))
				runEndSamples.push (i);
		}
		const runCount = runStartSamples.length;
		if (runCount === 0)
			return null;

		const groupFirstLines = [];
		for (let lineIndex = 0; lineIndex < lineCount; lineIndex++) {
			if ((lineIndex === 0) || (this.lineGroupIDs[lineIndex] !== this.lineGroupIDs[lineIndex - 1]))
				groupFirstLines.push (lineIndex);
		}
		const groupCount = groupFirstLines.length;
		groupFirstLines.push (lineCount);

		const groupDurations = new Float64Array (groupCount);
		for (let group = 0; group < groupCount; group++) {
			for (let lineIndex = groupFirstLines[group]; lineIndex < groupFirstLines[group + 1]; lineIndex++)
				groupDurations[group] += this.lineDurations[lineIndex];
		}

		let bounds = null;
		let boundsGroup = -1;
		const runLiesOnGroup = (run, group) => {
			const sample = runStartSamples[run];
			const px = points[sample * 2];
			const py = points[sample * 2 + 1];
			const firstLine = groupFirstLines[group];
			const lastLine = Math.min (groupFirstLines[group + 1], firstLine + LAYERVIEW_TIMING_MAXGROUPLINES);

			if (boundsGroup !== group) {
				bounds = { minx: Infinity, miny: Infinity, maxx: -Infinity, maxy: -Infinity };
				for (let lineIndex = firstLine; lineIndex < lastLine; lineIndex++) {
					for (let corner = 0; corner < 2; corner++) {
						const x = coords[lineIndex * 4 + corner * 2];
						const y = coords[lineIndex * 4 + corner * 2 + 1];
						bounds.minx = Math.min (bounds.minx, x);
						bounds.maxx = Math.max (bounds.maxx, x);
						bounds.miny = Math.min (bounds.miny, y);
						bounds.maxy = Math.max (bounds.maxy, y);
					}
				}
				boundsGroup = group;
			}
			if ((px < bounds.minx - tolerance) || (px > bounds.maxx + tolerance) || (py < bounds.miny - tolerance) || (py > bounds.maxy + tolerance))
				return false;

			for (let lineIndex = firstLine; lineIndex < lastLine; lineIndex++) {
				const arrayIndex = lineIndex * 4;
				if (LayerViewImpl.pointSegmentDistanceSq (px, py, coords[arrayIndex], coords[arrayIndex + 1], coords[arrayIndex + 2], coords[arrayIndex + 3]) < toleranceSq)
					return true;
			}
			return false;
		};

		const groupStarts = new Float64Array (groupCount).fill (NaN);
		const groupEnds = new Float64Array (groupCount).fill (NaN);
		let matchedGroups = 0;
		let nextRun = 0;
		for (let group = 0; (group < groupCount) && (nextRun < runCount); group++) {
			const lastCandidate = Math.min (nextRun + LAYERVIEW_TIMING_RUNLOOKAHEAD, runCount);
			for (let run = nextRun; run < lastCandidate; run++) {
				if (!runLiesOnGroup (run, group))
					continue;

				let lastRun = run;
				while ((lastRun + 1 < runCount) && runLiesOnGroup (lastRun + 1, group))
					lastRun++;

				groupStarts[group] = times[runStartSamples[run]];
				groupEnds[group] = times[runEndSamples[lastRun]];
				nextRun = lastRun + 1;
				matchedGroups++;
				break;
			}
		}

		if (matchedGroups === 0)
			return null;

		// Unmatched vectors share the time between the surrounding matched ones.
		let previousEnd = times[0];
		let group = 0;
		while (group < groupCount) {
			if (!Number.isNaN (groupStarts[group])) {
				previousEnd = Math.max (groupEnds[group], previousEnd);
				group++;
				continue;
			}

			let stretchEnd = group;
			let stretchDuration = 0.0;
			while ((stretchEnd < groupCount) && Number.isNaN (groupStarts[stretchEnd])) {
				stretchDuration += groupDurations[stretchEnd];
				stretchEnd++;
			}
			const nextStart = (stretchEnd < groupCount) ? groupStarts[stretchEnd] : times[sampleCount - 1];
			const available = Math.max (nextStart - previousEnd, 0.0);

			let currentTime = previousEnd;
			for (let stretchGroup = group; stretchGroup < stretchEnd; stretchGroup++) {
				const share = (stretchDuration > 0) ? groupDurations[stretchGroup] / stretchDuration : 1.0 / (stretchEnd - group);
				groupStarts[stretchGroup] = currentTime;
				currentTime += share * available;
				groupEnds[stretchGroup] = currentTime;
			}
			group = stretchEnd;
		}

		const lineStartTimes = new Float64Array (lineCount);
		let latestTime = -Infinity;
		for (let currentGroup = 0; currentGroup < groupCount; currentGroup++) {
			const groupDuration = groupDurations[currentGroup];
			const span = groupEnds[currentGroup] - groupStarts[currentGroup];
			let elapsed = 0.0;
			for (let lineIndex = groupFirstLines[currentGroup]; lineIndex < groupFirstLines[currentGroup + 1]; lineIndex++) {
				const startTime = groupStarts[currentGroup] + ((groupDuration > 0) ? (elapsed / groupDuration) * span : 0.0);
				// Keep the start times sorted for the binary search in applyTimeCap.
				latestTime = Math.max (latestTime, startTime);
				lineStartTimes[lineIndex] = latestTime;
				elapsed += this.lineDurations[lineIndex];
			}
		}

		this.timingMatchRatio = matchedGroups / groupCount;
		return lineStartTimes;
	}

	getTimelineDuration ()
	{
		return this.timelineDuration;
	}

	// Share (0..1) of marking vectors that were matched to the recording by updateTiming.
	getTimingMatchRatio ()
	{
		return this.timingMatchRatio;
	}

	getTimingSource ()
	{
		return this.timingSource;
	}

	// Draws only what happened up to fraction (0..1) of the layer timeline.
	setTimeCapFraction (fraction)
	{
		const value = Number (fraction);
		this.timeCapFraction = Number.isFinite (value) ? Math.min (Math.max (value, 0.0), 1.0) : 1.0;
		this.applyTimeCap ();

		if ((this.highlightLineIndex >= 0) && (this.highlightLineIndex >= this.visibleLineCount))
			this.clearHighlight ();
		this.RenderScene (true);
	}

	applyTimeCap ()
	{
		if (!this.glInstance)
			return;

		const capped = this.timeCapFraction < 1.0;
		const capTime = this.timeCapFraction * this.timelineDuration;

		const lineCount = Math.floor (this.linesCoordinates.length / 4);
		this.visibleLineCount = lineCount;
		if (capped && this.lineStartTimes && (this.lineStartTimes.length === lineCount))
			this.visibleLineCount = LayerViewImpl.countSortedBelow (this.lineStartTimes, capTime, true);

		const linesElement = this.glInstance.findElement ("layerdata_lines");
		if (linesElement && (typeof linesElement.setVisibleLineCount === "function"))
			linesElement.setVisibleLineCount (this.visibleLineCount);

		const pointCount = this.layerPointsArray ? Math.floor (this.layerPointsArray.length / 2) : 0;
		let visibleOriginalPoints = pointCount;
		if (capped && (pointCount > 0)) {
			const times = this.laser ? this.laser.time : null;
			if (times && (times.length === pointCount))
				visibleOriginalPoints = LayerViewImpl.countSortedBelow (times, capTime, true);
			else
				visibleOriginalPoints = Math.round (pointCount * this.timeCapFraction);
		}

		this.visiblePointCount = this.filteredToOriginalIndexMap
			? LayerViewImpl.countSortedBelow (this.filteredToOriginalIndexMap, visibleOriginalPoints, false)
			: visibleOriginalPoints;

		const pointsElement = this.glInstance.findElement ("layerdata_points");
		if (pointsElement && (typeof pointsElement.setDrawCount === "function"))
			pointsElement.setDrawCount (this.visiblePointCount);

		this.renderNeedsUpdate = true;
	}

	resolvePointIndex (filteredIndex)
	{
		if (this.filteredToOriginalIndexMap && filteredIndex >= 0
			&& filteredIndex < this.filteredToOriginalIndexMap.length) {
			return this.filteredToOriginalIndexMap[filteredIndex];
		}
		return filteredIndex;
	}

	getPointPosition (pointIndex)
	{
		if (this.layerPointsArray) {
			if (pointIndex >= 0) {
				let arrayIndex = pointIndex * 2;
				if (arrayIndex + 1 < this.layerPointsArray.length) {
					let xPosition = this.layerPointsArray[arrayIndex];
					let yPosition = this.layerPointsArray[arrayIndex + 1];
					return { x: xPosition, y: yPosition }
				}
			}

		}
		
		return null;
					
	}

	// Velocity in mm/s from the neighbouring samples. Uses the recorded "time" channel (ms)
	// when present, otherwise a fixed sample period of 10 microseconds.
	getPointVelocity (pointIndex)
	{
		let dT = 1 / 100000;
		
		if (this.layerPointsArray) {
			if (pointIndex > 0) {
				let arrayIndex = pointIndex * 2;
				if (arrayIndex + 3 < this.layerPointsArray.length) {
					const times = this.laser ? this.laser.time : null;
					if (times && (pointIndex + 1 < times.length)) {
						const sampleSpan = (times[pointIndex + 1] - times[pointIndex - 1]) * 0.001;
						if (sampleSpan > 0)
							dT = sampleSpan * 0.5;
					}
					let vX = (this.layerPointsArray[arrayIndex + 2] - this.layerPointsArray[arrayIndex - 2]) / (2 * dT);
					let vY = (this.layerPointsArray[arrayIndex + 3] - this.layerPointsArray[arrayIndex - 1]) / (2 * dT);
					let velocity = Math.sqrt (vX * vX + vY * vY);
					return velocity;
				}
			}

		}
		
		return 0.0;
					
	}

	getPointAcceleration (pointIndex)
	{
		let dT = 1 / 100000; 
		
		if (this.layerPointsArray) {
			if (pointIndex > 0) {
				let arrayIndex = pointIndex * 2;
				if (arrayIndex + 3 < this.layerPointsArray.length) {
					let aX = (this.layerPointsArray[arrayIndex + 2] - 2 * this.layerPointsArray[arrayIndex] + this.layerPointsArray[arrayIndex - 2]) / (dT * dT);
					let aY = (this.layerPointsArray[arrayIndex + 3] - 2 * this.layerPointsArray[arrayIndex + 1] + this.layerPointsArray[arrayIndex - 1]) / (dT * dT);
					let accel = Math.sqrt (aX * aX + aY * aY);
					return { ax: aX,  ay: aY, a: accel }
				}
			}

		} 
		
		return null;
					
	}


	getPointJerk (pointIndex)
	{
		let dT = 1 / 100000;
		
		if (this.layerPointsArray) {
			if (pointIndex > 1) {
				let arrayIndex = pointIndex * 2;
				if (arrayIndex + 5 < this.layerPointsArray.length) {
					let aXPrev = (this.layerPointsArray[arrayIndex] - 2 * this.layerPointsArray[arrayIndex - 2] + this.layerPointsArray[arrayIndex - 4]) / (dT * dT);
					let aYPrev = (this.layerPointsArray[arrayIndex + 1] - 2 * this.layerPointsArray[arrayIndex - 1] + this.layerPointsArray[arrayIndex - 3]) / (dT * dT);
					let aXNext = (this.layerPointsArray[arrayIndex + 2] - 2 * this.layerPointsArray[arrayIndex] + this.layerPointsArray[arrayIndex - 2]) / (dT * dT);
					let aYNext = (this.layerPointsArray[arrayIndex + 3] - 2 * this.layerPointsArray[arrayIndex + 1] + this.layerPointsArray[arrayIndex - 1]) / (dT * dT);
					let jX = (aXNext - aXPrev) / (2 * dT);
					let jY = (aYNext - aYPrev) / (2 * dT);
					let jerk = Math.sqrt (jX * jX + jY * jY);
					return { jx: jX,  jy: jY, j: jerk }
				}
			}

		}
		
		return null;
					
	}

	getPointPower (pointIndex)
	{
		if (this.laser && this.laser.power) {
			if (pointIndex >= 0 && pointIndex < this.laser.power.length) {
				return this.laser.power[pointIndex];				
			}
		}
		return null;
	}

	updateLoadedLayer () {
		if (!this.glInstance) 
			return;
		
        this.glInstance.removeElement("layerdata_lines");
        // The highlighted line references indices into linesCoordinates, which are
        // about to be regenerated, so drop any stale highlight overlay.
        this.removeHighlightGeometry ();
        this.highlightLineIndex = -1;
		
		this.linesCoordinates = [];
		this.segmentProperties = [];
		this.lineGroupIDs = null;
		this.lineDurations = null;
		this.linePowers = [];
		this.lineSupportPoints = new Map ();

		if (this.layerSegmentsArray) {
				
			let segmentsArray = this.layerSegmentsArray;
			var segmentCount = segmentsArray.length;
			var segmentIndex;
			
			let vertexcolors = [];
			// One group per polyline and per hatch line: each is marked with one laser-on run.
			let lineGroupIDs = [];
			let lineDurations = [];
			let groupID = 0;
			let pushLine = (x1, y1, x2, y2, segmentData, segmentColor, markSpeed, power1, power2, supportPoints) => {
				if (supportPoints && (supportPoints.length > 0))
					this.lineSupportPoints.set (this.segmentProperties.length, supportPoints);
				this.linesCoordinates.push(x1, y1, x2, y2);
				this.segmentProperties.push (segmentData);
				this.linePowers.push (power1, power2);
				vertexcolors.push (segmentColor);
				lineGroupIDs.push (groupID);
				lineDurations.push (Math.hypot (x2 - x1, y2 - y1) / markSpeed * 1000.0);
			};
			let pointPower = (powers, pointIndex) => {
				return (powers && (pointIndex < powers.length)) ? Number (powers[pointIndex]) : NaN;
			};

			for (segmentIndex = 0; segmentIndex < segmentCount; segmentIndex++) {
				var segment = segmentsArray[segmentIndex];
				let partUUID = (typeof segment.partuuid === "string") ? segment.partuuid.toLowerCase () : "";
				let partDisabled = this.layerDisabledParts.has (partUUID);
				let segmentColor = partDisabled ? LAYERVIEW_DISABLEDPARTCOLOR : segment.color;
				let segmentData = {
					type: segment.type,
					laserpower: segment.laserpower,
					laserspeed: segment.laserspeed,
					profilename: segment.profilename,
					partid: segment.partid,
					partuuid: partUUID,
					partname: this.layerPartNames.get (partUUID) || "",
					partdisabled: partDisabled,
					laserindex: segment.laserindex,
					powermode: (typeof segment.powermode === "string") ? segment.powermode : ""
				}
				let markSpeed = (Number (segment.laserspeed) > 0) ? Number (segment.laserspeed) : LAYERVIEW_DEFAULTMARKSPEED;
				let powers = Array.isArray (segment.powers) ? segment.powers : null;

				if ((segment.type === "loop") || (segment.type === "polyline")) {
					var pointCount = segment.points.length;
					var pointIndex;
					
					var oldx = segment.points[0].x;
					var oldy = segment.points[0].y;
					groupID++;

					for (pointIndex = 1; pointIndex < pointCount; pointIndex++) {
						var x = segment.points[pointIndex].x;
						var y = segment.points[pointIndex].y;

						if ((oldx === x) && (oldy === y)) {
							oldx = x;
							oldy = y;
							continue;
						}

						// The factor of a polyline point applies to the line that ends in it.
						let linePower = pointPower (powers, pointIndex);
						pushLine (oldx, oldy, x, y, segmentData, segmentColor, markSpeed, linePower, linePower, null);

						oldx = x;
						oldy = y;
					}
				}

				if (segment.type === "hatch") {
					var lineCount = segment.points.length / 2;
					var lineIndex;
					let subCounts = Array.isArray (segment.powersubcounts) ? segment.powersubcounts : null;
					let subValues = Array.isArray (segment.powersubs) ? segment.powersubs : null;
					let subOffset = 0;

					for (lineIndex = 0; lineIndex < lineCount; lineIndex++) {
						var x1 = segment.points[lineIndex * 2].x;
						var y1 = segment.points[lineIndex * 2].y;
						var x2 = segment.points[lineIndex * 2 + 1].x;
						var y2 = segment.points[lineIndex * 2 + 1].y;

						let supportPoints = null;
						let subCount = (subCounts && (lineIndex < subCounts.length)) ? Number (subCounts[lineIndex]) : 0;
						if ((subCount > 0) && subValues && ((subOffset + subCount) * 2 <= subValues.length)) {
							supportPoints = [];
							for (let subIndex = 0; subIndex < subCount; subIndex++) {
								let valueIndex = (subOffset + subIndex) * 2;
								supportPoints.push ({ t: Number (subValues[valueIndex]), power: Number (subValues[valueIndex + 1]) });
							}
						}
						subOffset += subCount;

						if ((x1 === x2) && (y1 === y2)) {
							continue;
						}

						groupID++;
						pushLine (x1, y1, x2, y2, segmentData, segmentColor, markSpeed,
							pointPower (powers, lineIndex * 2), pointPower (powers, lineIndex * 2 + 1), supportPoints);
					}

				}
			}

			this.lineGroupIDs = lineGroupIDs;
			this.lineDurations = lineDurations;
			
			if (this.toolpathVisible) {
				let thickness = this.lineScaleLevel * 0.3;
				this.glInstance.add2DLineGeometry("layerdata_lines", this.linesCoordinates, 60, thickness, 0x000000, vertexcolors);
			}
		}

		this.updateTiming ();

		this.updateTransform();
		this.RenderScene(true);		

    }

    Drag(deltaX, deltaY) {

        // console.log("dX: " + deltaX + " dY: " + deltaY);

        this.transform.x = this.transform.x + deltaX;
        this.transform.y = this.transform.y + deltaY;

        this.updateTransform();

    }

    SetAbsoluteScaling(newScaling, centerx, centery) {

        if (!centerx) {
            centerx = this.twoinstance.width * 0.5;
        }

        if (!centery) {
            centery = this.twoinstance.height * 0.5;
        }

        this.ScaleRelative(newScaling / this.transform.scaling, centerx, centery);

    }

    ScaleRelative(factor, centerx, centery) {

        if (!centerx) {
            centerx = this.currentSize.viewPortWidth * 0.5;
        }

        if (!centery) {
            centery = this.currentSize.viewPortHeight * 0.5;
        }

        this.transform.x = this.transform.x - centerx;
        this.transform.y = this.transform.y - centery;

		// console.log("oldscaling: " + this.transform.scaling);

        var oldScaling = this.transform.scaling;
        this.transform.scaling = oldScaling * factor;
        if (this.transform.scaling < LAYERVIEW_MINSCALING)
            this.transform.scaling = LAYERVIEW_MINSCALING;

        if (this.transform.scaling > LAYERVIEW_MAXSCALING)
            this.transform.scaling = LAYERVIEW_MAXSCALING;

		// console.log("newscaling: " + this.transform.scaling);

        factor = this.transform.scaling / oldScaling;

        this.transform.x = Math.ceil(this.transform.x * factor + centerx);
        this.transform.y = Math.ceil(this.transform.y * factor + centery);

        this.updateTransform();
		
    }
	
	CenterOnRectangle (minx, miny, maxx, maxy)
	{
		var sizex = maxx - minx;
		var sizey = maxy - miny;
		
		if ((this.currentSize.viewPortWidth > 0) && (this.currentSize.viewPortHeight > 0)) {
			if ((sizex > 0) && (sizey > 0)) {
				
				var scalingx = (this.currentSize.viewPortWidth / sizex);
				var scalingy = (this.currentSize.viewPortHeight / sizey);
				
				var newcenterx = (maxx + minx) * 0.5;
				var newcentery = (maxy + miny) * 0.5;
				
				var viewcenterx = this.currentSize.viewPortWidth * 0.5;
				var viewcentery = this.currentSize.viewPortHeight * 0.5;
								
				this.transform.x = viewcenterx - this.transform.scaling * newcenterx;
				this.transform.y = viewcentery + this.transform.scaling * newcentery;
								
				var newScaling;
				if (scalingx > scalingy) {
					newScaling = scalingy;
				} else {
					newScaling = scalingx;
				}
				
				this.SetAbsoluteScaling (newScaling, viewcenterx, viewcentery);
			
			}
		}
		
		
	}
	
    RenderScene (forceRender = false) 
	{
		if (forceRender) {
			this.renderNeedsUpdate = true;
		}
		
		if (this.renderNeedsUpdate && this.glInstance) {
			this.glInstance.renderScene ();
			this.renderNeedsUpdate = false;
		}			
		
	} 
	
	SetBuildPlateSVG (url) {
		if (this.glInstance) {
			if (url) {
				this.svgImage = this.glInstance.addSVGImage("buildplate", url, 35, true, true);
				this.updateTransform ();
			} else {
				this.glInstance.removeElement ("buildplate");
			}
		}
	} 
		
	setOrigin (x, y)
	{
		if (isNaN (x) || isNaN (y))
			return;
		
		this.origin.x = x;
		this.origin.y = y;
	}

	// Converts a viewport pixel coordinate (relative to the canvas, in CSS pixels)
	// into machine/build-plate coordinates in millimeters. The build plate places
	// machine (0,0) at (transform.x, transform.y) with the Y axis inverted, so this
	// is the exact inverse of that mapping. Returns null if no valid scaling exists.
	/**
	 * @param {number} screenX
	 * @param {number} screenY
	 */
	screenToMachine (screenX, screenY)
	{
		if (!this.transform)
			return null;

		const scaling = this.transform.scaling;
		if (!(scaling > 0) || isNaN (screenX) || isNaN (screenY))
			return null;

		return {
			x: (screenX - this.transform.x) / scaling,
			y: (this.transform.y - screenY) / scaling
		};
	}

	// Maps a raw toolpath point (as stored in linesCoordinates, i.e. before the
	// placement/rotation/origin transform) into machine/build-plate coordinates
	// in millimeters. This is the exact forward of screenToMachine so that a
	// cursor position and a segment endpoint can be compared in the same frame.
	/**
	 * @param {number} toolpathX
	 * @param {number} toolpathY
	 */
	toolpathPointToMachine (toolpathX, toolpathY)
	{
		const angleInRadians = this.transformAngleDegrees * Math.PI / 180.0;
		const cosine = Math.cos (angleInRadians);
		const sine = Math.sin (angleInRadians);
		const placement = this.transformToolpathPoint (0, 0);

		return {
			x: this.origin.x + placement.x + (toolpathX * cosine + toolpathY * sine),
			y: this.origin.y + placement.y + (-toolpathX * sine + toolpathY * cosine)
		};
	}

	// Inverse of screenToMachine: machine coordinates (mm) to viewport pixels.
	/**
	 * @param {number} machineX
	 * @param {number} machineY
	 */
	machineToScreen (machineX, machineY)
	{
		if (!this.transform)
			return null;

		return {
			x: this.transform.x + machineX * this.transform.scaling,
			y: this.transform.y - machineY * this.transform.scaling
		};
	}

	// Returns one axis-aligned bounding box per part of the loaded layer, in
	// machine coordinates (mm). Parts are identified by their build item UUID
	// (lower case), which is stable across layers, unlike the layer-local partid.
	// Segments without a UUID (older servers) are grouped by partid instead;
	// those boxes have an empty uuid.
	getPartBoundingBoxes ()
	{
		if (!this.layerSegmentsArray)
			return [];

		const cacheKey = [this.transformAngleDegrees, this.rotationCenter.x, this.rotationCenter.y,
			this.translation.x, this.translation.y, this.origin.x, this.origin.y].join ("|");
		if (this.partBoundsCache && (this.partBoundsCacheKey === cacheKey))
			return this.partBoundsCache;

		const angleInRadians = this.transformAngleDegrees * Math.PI / 180.0;
		const cosine = Math.cos (angleInRadians);
		const sine = Math.sin (angleInRadians);
		const placement = this.transformToolpathPoint (0, 0);
		const offsetX = this.origin.x + placement.x;
		const offsetY = this.origin.y + placement.y;

		const boxesByKey = new Map ();
		for (const segment of this.layerSegmentsArray) {
			if (!segment || !Array.isArray (segment.points))
				continue;

			let partUUID = (typeof segment.partuuid === "string") ? segment.partuuid.toLowerCase () : "";
			if (partUUID === "00000000-0000-0000-0000-000000000000")
				partUUID = "";

			let key;
			let name;
			if (partUUID !== "") {
				key = partUUID;
				name = this.layerPartNames.get (partUUID) || "";
			} else if (Number.isInteger (segment.partid)) {
				key = "partid:" + segment.partid;
				name = "Part " + segment.partid;
			} else {
				continue;
			}

			let box = boxesByKey.get (key);
			if (!box) {
				box = {
					key: key,
					uuid: partUUID,
					name: name,
					disabled: (partUUID !== "") && this.layerDisabledParts.has (partUUID),
					minx: Infinity, miny: Infinity, maxx: -Infinity, maxy: -Infinity
				};
				boxesByKey.set (key, box);
			}

			for (const point of segment.points) {
				const machineX = offsetX + (point.x * cosine + point.y * sine);
				const machineY = offsetY + (-point.x * sine + point.y * cosine);
				if (machineX < box.minx) box.minx = machineX;
				if (machineX > box.maxx) box.maxx = machineX;
				if (machineY < box.miny) box.miny = machineY;
				if (machineY > box.maxy) box.maxy = machineY;
			}
		}

		this.partBoundsCache = Array.from (boxesByKey.values ()).filter ((box) => (box.minx <= box.maxx) && (box.miny <= box.maxy));
		this.partBoundsCacheKey = cacheKey;
		return this.partBoundsCache;
	}

	// Squared distance from point (px,py) to the line segment (ax,ay)-(bx,by).
	static pointSegmentDistanceSq (px, py, ax, ay, bx, by)
	{
		const dx = bx - ax;
		const dy = by - ay;
		const lengthSq = dx * dx + dy * dy;

		let t = 0.0;
		if (lengthSq > 0.0) {
			t = ((px - ax) * dx + (py - ay) * dy) / lengthSq;
			if (t < 0.0) t = 0.0;
			if (t > 1.0) t = 1.0;
		}

		const cx = ax + t * dx;
		const cy = ay + t * dy;
		const ex = px - cx;
		const ey = py - cy;
		return ex * ex + ey * ey;
	}

	// Finds the toolpath line segment closest to the given machine coordinate
	// (mm). Returns the associated segment properties (laser power/speed, profile
	// name, part/laser index, type) when within toleranceMM, otherwise null.
	/**
	 * @param {number} machineX
	 * @param {number} machineY
	 * @param {number} toleranceMM
	 */
	pickSegmentAtMachinePoint (machineX, machineY, toleranceMM)
	{
		if (!this.toolpathVisible || !this.linesCoordinates || !this.segmentProperties)
			return null;

		const coords = this.linesCoordinates;
		const lineCount = Math.min (Math.floor (coords.length / 4), this.visibleLineCount);
		if (lineCount <= 0)
			return null;

		const angleInRadians = this.transformAngleDegrees * Math.PI / 180.0;
		const cosine = Math.cos (angleInRadians);
		const sine = Math.sin (angleInRadians);
		const placement = this.transformToolpathPoint (0, 0);
		const baseX = this.origin.x + placement.x;
		const baseY = this.origin.y + placement.y;

		let bestDistanceSq = toleranceMM * toleranceMM;
		let bestIndex = -1;

		for (let lineIndex = 0; lineIndex < lineCount; lineIndex++) {
			const arrayIndex = lineIndex * 4;
			const px1 = coords[arrayIndex];
			const py1 = coords[arrayIndex + 1];
			const px2 = coords[arrayIndex + 2];
			const py2 = coords[arrayIndex + 3];

			const ax = baseX + (px1 * cosine + py1 * sine);
			const ay = baseY + (-px1 * sine + py1 * cosine);
			const bx = baseX + (px2 * cosine + py2 * sine);
			const by = baseY + (-px2 * sine + py2 * cosine);

			const distanceSq = LayerViewImpl.pointSegmentDistanceSq (machineX, machineY, ax, ay, bx, by);
			if (distanceSq < bestDistanceSq) {
				bestDistanceSq = distanceSq;
				bestIndex = lineIndex;
			}
		}

		if (bestIndex >= 0 && bestIndex < this.segmentProperties.length) {
			const power1 = this.linePowers[bestIndex * 2];
			const power2 = this.linePowers[bestIndex * 2 + 1];
			const supportPoints = this.lineSupportPoints.get (bestIndex);
			return {
				...this.segmentProperties[bestIndex],
				lineIndex: bestIndex,
				power1: Number.isFinite (power1) ? power1 : null,
				power2: Number.isFinite (power2) ? power2 : null,
				supportpoints: supportPoints ? supportPoints.map ((point) => ({ ...point })) : null
			};
		}

		return null;
	}

	// Draws (or removes, when lineIndex < 0) a highlight overlay on top of a
	// single toolpath line so the segment currently under the cursor stands out.
	// The overlay shares the toolpath transform applied in updateTransform().
	/**
	 * @param {number} lineIndex
	 */
	setHighlightLine (lineIndex)
	{
		if (!this.glInstance)
			return;

		if (lineIndex === undefined || lineIndex === null || lineIndex < 0 || !this.linesCoordinates || !this.toolpathVisible ||
			((lineIndex * 4 + 3) >= this.linesCoordinates.length))
			lineIndex = -1;

		// Hovering moves along the same line most of the time; keep the overlay then.
		if ((lineIndex === this.highlightLineIndex) && ((lineIndex < 0) || (this.highlightScaling === this.transform.scaling)))
			return;

		this.highlightLineIndex = lineIndex;
		this.buildHighlightGeometry ();
		this.positionHighlightGeometry ();
		this.RenderScene (true);
	}

	removeHighlightGeometry ()
	{
		this.glInstance.removeElement ("layerdata_highlight");
		this.glInstance.removeElement ("layerdata_highlightarrow");
		this.glInstance.removeElement ("layerdata_highlightpoints");
		this.highlightSupportPoints = null;
	}

	// Creates the overlay of highlightLineIndex: the line with an arrow head at its end point
	// (the marking direction) and, for nonlinear power ramps, its support points including
	// both end points. Uses the toolpath coordinates of linesCoordinates.
	buildHighlightGeometry ()
	{
		this.removeHighlightGeometry ();
		this.highlightScaling = this.transform.scaling;

		const lineIndex = this.highlightLineIndex;
		const arrayIndex = lineIndex * 4;
		if ((lineIndex < 0) || (arrayIndex + 3 >= this.linesCoordinates.length))
			return;

		const x1 = this.linesCoordinates[arrayIndex];
		const y1 = this.linesCoordinates[arrayIndex + 1];
		const x2 = this.linesCoordinates[arrayIndex + 2];
		const y2 = this.linesCoordinates[arrayIndex + 3];
		// Noticeably thicker than the regular preview lines (lineScaleLevel * 0.3)
		// and drawn at a higher z so it sits above lines (60) and points (61).
		const highlightThickness = this.lineScaleLevel * 1.1;
		this.glInstance.add2DLineGeometry ("layerdata_highlight", [x1, y1, x2, y2], 63, highlightThickness, LAYERVIEW_HIGHLIGHTCOLOR, [LAYERVIEW_HIGHLIGHTCOLOR]);

		const length = Math.hypot (x2 - x1, y2 - y1);
		const scaling = (this.transform.scaling > 0) ? this.transform.scaling : 1.0;
		const arrowLength = Math.min (Math.max (LAYERVIEW_HIGHLIGHTARROWPIXELS / scaling, highlightThickness * LAYERVIEW_HIGHLIGHTARROWTHICKNESSFACTOR), length * 0.5);
		if (arrowLength > 0) {
			const backX = (x1 - x2) / length;
			const backY = (y1 - y2) / length;
			const cosine = Math.cos (LAYERVIEW_HIGHLIGHTARROWANGLE);
			const sine = Math.sin (LAYERVIEW_HIGHLIGHTARROWANGLE);
			const arrowCoordinates = [
				x2, y2, x2 + arrowLength * (backX * cosine - backY * sine), y2 + arrowLength * (backX * sine + backY * cosine),
				x2, y2, x2 + arrowLength * (backX * cosine + backY * sine), y2 + arrowLength * (-backX * sine + backY * cosine)
			];
			this.glInstance.add2DLineGeometry ("layerdata_highlightarrow", arrowCoordinates, 63, highlightThickness * LAYERVIEW_HIGHLIGHTARROWWINGFACTOR,
				LAYERVIEW_HIGHLIGHTCOLOR, [LAYERVIEW_HIGHLIGHTCOLOR, LAYERVIEW_HIGHLIGHTCOLOR]);
		}

		const supportPoints = this.lineSupportPoints.get (lineIndex);
		if (!supportPoints)
			return;

		const knots = [{ t: 0.0, power: this.linePowers[lineIndex * 2] }, ...supportPoints, { t: 1.0, power: this.linePowers[lineIndex * 2 + 1] }];
		const knotCoordinates = new Float32Array (knots.length * 2);
		const knotColors = [];
		knots.forEach ((knot, knotIndex) => {
			knotCoordinates[knotIndex * 2] = x1 + knot.t * (x2 - x1);
			knotCoordinates[knotIndex * 2 + 1] = y1 + knot.t * (y2 - y1);
			knotColors.push (((knotIndex === 0) || (knotIndex === knots.length - 1)) ? LAYERVIEW_SUPPORTENDPOINTCOLOR : LAYERVIEW_SUPPORTPOINTCOLOR);
		});

		this.glInstance.add2DScreenPointsGeometry ("layerdata_highlightpoints", knotCoordinates, 64,
			LAYERVIEW_SUPPORTPOINTPIXELS * this.glInstance.getPixelRatio (), LAYERVIEW_SUPPORTPOINTCOLOR, knotColors);
		this.highlightSupportPoints = { knots, x1, y1, x2, y2, length };
	}

	positionHighlightGeometry ()
	{
		const placementOffset = this.transformToolpathPoint (0, 0);
		const positionX = this.transform.x + (this.origin.x + placementOffset.x) * this.transform.scaling;
		const positionY = this.transform.y - (this.origin.y + placementOffset.y) * this.transform.scaling;

		for (const identifier of ["layerdata_highlight", "layerdata_highlightarrow", "layerdata_highlightpoints"]) {
			const element = this.glInstance.findElement (identifier);
			if (element) {
				element.setPositionXY (positionX, positionY);
				element.setScaleXY (this.transform.scaling, - this.transform.scaling);
				element.setRotationZ (-this.transformAngleDegrees * Math.PI / 180.0);
			}
		}
	}

	// Support point of the highlighted nonlinear ramp under the viewport position, with its
	// neighbours, so the piece of the ramp on either side can be shown; null if none.
	/**
	 * @param {number} screenX
	 * @param {number} screenY
	 * @param {number} [pixelTolerance]
	 */
	pickHighlightSupportPoint (screenX, screenY, pixelTolerance = 3)
	{
		if (!this.glInstance || !this.highlightSupportPoints)
			return null;

		const pointsElement = this.glInstance.findElement ("layerdata_highlightpoints");
		if (!pointsElement || (typeof pointsElement.findNearestPoint !== "function"))
			return null;

		const knotIndex = pointsElement.findNearestPoint (screenX, screenY, pixelTolerance);
		const { knots, x1, y1, x2, y2, length } = this.highlightSupportPoints;
		if ((knotIndex < 0) || (knotIndex >= knots.length))
			return null;

		const describe = (index) => {
			if ((index < 0) || (index >= knots.length))
				return null;
			const knot = knots[index];
			return {
				t: knot.t,
				power: Number.isFinite (knot.power) ? knot.power : null,
				distance: knot.t * length
			};
		};

		const knot = knots[knotIndex];
		return {
			lineIndex: this.highlightLineIndex,
			index: knotIndex,
			count: knots.length,
			x: x1 + knot.t * (x2 - x1),
			y: y1 + knot.t * (y2 - y1),
			length,
			...describe (knotIndex),
			previous: describe (knotIndex - 1),
			next: describe (knotIndex + 1)
		};
	}

	clearHighlight ()
	{
		this.setHighlightLine (-1);
	}

	// Convenience picker that works directly in viewport pixel space. The pixel
	// tolerance is converted to millimeters using the current zoom so the hover
	// hit area stays constant on screen regardless of zoom level.
	/**
	 * @param {number} screenX
	 * @param {number} screenY
	 * @param {number} [pixelTolerance]
	 */
	pickSegmentAtScreenPoint (screenX, screenY, pixelTolerance = 6)
	{
		const machine = this.screenToMachine (screenX, screenY);
		if (!machine)
			return null;

		const scaling = this.transform.scaling;
		if (!(scaling > 0))
			return null;

		return this.pickSegmentAtMachinePoint (machine.x, machine.y, pixelTolerance / scaling);
	}

	setTransformAngle (angleInDegrees)
	{
		this.setCoordinateTransform(angleInDegrees, this.rotationCenter.x, this.rotationCenter.y, this.translation.x, this.translation.y);
	}

	setCoordinateTransform (angleInDegrees, rotationCenterX, rotationCenterY, translationX, translationY)
	{
		if (isNaN(angleInDegrees) || isNaN(rotationCenterX) || isNaN(rotationCenterY) || isNaN(translationX) || isNaN(translationY))
			return;

		this.transformAngleDegrees = Number(angleInDegrees);
		this.rotationCenter.x = Number(rotationCenterX);
		this.rotationCenter.y = Number(rotationCenterY);
		this.translation.x = Number(translationX);
		this.translation.y = Number(translationY);
		this.updateTransform();
	}
}



export default LayerViewImpl;
