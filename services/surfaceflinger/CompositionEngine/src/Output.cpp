/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/stringprintf.h>
#include <compositionengine/CompositionEngine.h>
#include <compositionengine/DisplayColorProfile.h>
#include <compositionengine/RenderSurface.h>
#include <compositionengine/impl/Output.h>
#include <ui/DebugUtils.h>

namespace android::compositionengine {

Output::~Output() = default;

namespace impl {

Output::Output(const CompositionEngine& compositionEngine)
      : mCompositionEngine(compositionEngine) {}

Output::~Output() = default;

const CompositionEngine& Output::getCompositionEngine() const {
    return mCompositionEngine;
}

bool Output::isValid() const {
    return mDisplayColorProfile && mDisplayColorProfile->isValid() && mRenderSurface &&
            mRenderSurface->isValid();
}

const std::string& Output::getName() const {
    return mName;
}

void Output::setName(const std::string& name) {
    mName = name;
}

void Output::setCompositionEnabled(bool enabled) {
    if (mState.isEnabled == enabled) {
        return;
    }

    mState.isEnabled = enabled;
    dirtyEntireOutput();
}

void Output::setProjection(const ui::Transform& transform, int32_t orientation, const Rect& frame,
                           const Rect& viewport, const Rect& scissor, bool needsFiltering) {
    mState.transform = transform;
    mState.orientation = orientation;
    mState.scissor = scissor;
    mState.frame = frame;
    mState.viewport = viewport;
    mState.needsFiltering = needsFiltering;

    dirtyEntireOutput();
}

// TODO(lpique): Rename setSize() once more is moved.
void Output::setBounds(const ui::Size& size) {
    mRenderSurface->setDisplaySize(size);
    // TODO(lpique): Rename mState.size once more is moved.
    mState.bounds = Rect(mRenderSurface->getSize());

    dirtyEntireOutput();
}

void Output::setLayerStackFilter(uint32_t layerStackId, bool isInternal) {
    mState.layerStackId = layerStackId;
    mState.layerStackInternal = isInternal;

    dirtyEntireOutput();
}

void Output::setColorTransform(const mat4& transform) {
    const bool isIdentity = (transform == mat4());

    mState.colorTransform =
            isIdentity ? HAL_COLOR_TRANSFORM_IDENTITY : HAL_COLOR_TRANSFORM_ARBITRARY_MATRIX;
}

void Output::setColorMode(ui::ColorMode mode, ui::Dataspace dataspace,
                          ui::RenderIntent renderIntent) {
    mState.colorMode = mode;
    mState.dataspace = dataspace;
    mState.renderIntent = renderIntent;

    mRenderSurface->setBufferDataspace(dataspace);

    ALOGV("Set active color mode: %s (%d), active render intent: %s (%d)",
          decodeColorMode(mode).c_str(), mode, decodeRenderIntent(renderIntent).c_str(),
          renderIntent);
}

void Output::dump(std::string& out) const {
    using android::base::StringAppendF;

    StringAppendF(&out, "   Composition Output State: [\"%s\"]", mName.c_str());

    out.append("\n   ");

    dumpBase(out);
}

void Output::dumpBase(std::string& out) const {
    mState.dump(out);

    if (mDisplayColorProfile) {
        mDisplayColorProfile->dump(out);
    } else {
        out.append("    No display color profile!\n");
    }

    if (mRenderSurface) {
        mRenderSurface->dump(out);
    } else {
        out.append("    No render surface!\n");
    }
}

compositionengine::DisplayColorProfile* Output::getDisplayColorProfile() const {
    return mDisplayColorProfile.get();
}

void Output::setDisplayColorProfile(std::unique_ptr<compositionengine::DisplayColorProfile> mode) {
    mDisplayColorProfile = std::move(mode);
}

void Output::setDisplayColorProfileForTest(
        std::unique_ptr<compositionengine::DisplayColorProfile> mode) {
    mDisplayColorProfile = std::move(mode);
}

compositionengine::RenderSurface* Output::getRenderSurface() const {
    return mRenderSurface.get();
}

void Output::setRenderSurface(std::unique_ptr<compositionengine::RenderSurface> surface) {
    mRenderSurface = std::move(surface);
    mState.bounds = Rect(mRenderSurface->getSize());

    dirtyEntireOutput();
}

void Output::setRenderSurfaceForTest(std::unique_ptr<compositionengine::RenderSurface> surface) {
    mRenderSurface = std::move(surface);
}

const OutputCompositionState& Output::getState() const {
    return mState;
}

OutputCompositionState& Output::editState() {
    return mState;
}

Region Output::getDirtyRegion(bool repaintEverything) const {
    Region dirty(mState.viewport);
    if (!repaintEverything) {
        dirty.andSelf(mState.dirtyRegion);
    }
    return dirty;
}

bool Output::belongsInOutput(uint32_t layerStackId, bool internalOnly) const {
    // The layerStackId's must match, and also the layer must not be internal
    // only when not on an internal output.
    return (layerStackId == mState.layerStackId) && (!internalOnly || mState.layerStackInternal);
}

void Output::dirtyEntireOutput() {
    mState.dirtyRegion.set(mState.bounds);
}

} // namespace impl
} // namespace android::compositionengine
