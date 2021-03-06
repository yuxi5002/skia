/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrCCAtlas_DEFINED
#define GrCCAtlas_DEFINED

#include "GrAllocator.h"
#include "GrNonAtomicRef.h"
#include "GrResourceKey.h"
#include "GrTexture.h"
#include "SkRefCnt.h"
#include "SkSize.h"

class GrOnFlushResourceProvider;
class GrRenderTargetContext;
class GrTextureProxy;
struct SkIPoint16;
struct SkIRect;

/**
 * This class implements a dynamic size GrRectanizer that grows until it reaches the implementation-
 * dependent max texture size. When finalized, it also creates and stores a GrTextureProxy for the
 * underlying atlas.
 */
class GrCCAtlas {
public:
    // As long as GrSurfaceOrigin exists, we just have to decide on one for the atlas texture.
    static constexpr GrSurfaceOrigin kTextureOrigin = kTopLeft_GrSurfaceOrigin;
    static constexpr int kPadding = 1;  // Amount of padding below and to the right of each path.

    // This struct encapsulates the minimum and desired requirements for an atlas, as well as an
    // approximate number of pixels to help select a good initial size.
    struct Specs {
        int fMaxPreferredTextureSize = 0;
        int fMinTextureSize = 0;
        int fMinWidth = 0;  // If there are 100 20x10 paths, this should be 20.
        int fMinHeight = 0;  // If there are 100 20x10 paths, this should be 10.
        int fApproxNumPixels = 0;

        // Add space for a rect in the desired atlas specs.
        void accountForSpace(int width, int height);
    };

    GrCCAtlas(GrPixelConfig, const Specs&, const GrCaps&);
    ~GrCCAtlas();

    GrTextureProxy* textureProxy() const { return fTextureProxy.get(); }
    int currentWidth() const { return fWidth; }
    int currentHeight() const { return fHeight; }

    // Attempts to add a rect to the atlas. If successful, returns the integer offset from
    // device-space pixels where the path will be drawn, to atlas pixels where its mask resides.
    bool addRect(const SkIRect& devIBounds, SkIVector* atlasOffset);
    const SkISize& drawBounds() { return fDrawBounds; }

    // This is an optional space for the caller to jot down which user-defined batch to use when
    // they render the content of this atlas.
    void setUserBatchID(int id);
    int getUserBatchID() const { return fUserBatchID; }

    // Manages a unique resource cache key that gets assigned to the atlas texture. The unique key
    // does not get assigned to the texture proxy until it is instantiated.
    const GrUniqueKey& getOrAssignUniqueKey(GrOnFlushResourceProvider*);
    const GrUniqueKey& uniqueKey() const { return fUniqueKey; }

    // An object for simple bookkeeping on the atlas texture once it has a unique key. In practice,
    // we use it to track the percentage of the original atlas pixels that could still ever
    // potentially be reused (i.e., those which still represent an extant path). When the percentage
    // of useful pixels drops below 50%, the entire texture is purged from the resource cache.
    struct CachedAtlasInfo : public GrNonAtomicRef<CachedAtlasInfo> {
        int fNumPathPixels = 0;
        int fNumInvalidatedPathPixels = 0;
        bool fIsPurgedFromResourceCache = false;
    };
    sk_sp<CachedAtlasInfo> refOrMakeCachedAtlasInfo();

    // Instantiates our texture proxy for the atlas and returns a pre-cleared GrRenderTargetContext
    // that the caller may use to render the content. After this call, it is no longer valid to call
    // addRect(), setUserBatchID(), or this method again.
    //
    // 'backingTexture', if provided, is a renderable texture with which to instantiate our proxy.
    // If null then we will create a texture using the resource provider. The purpose of this param
    // is to provide a guaranteed way to recycle a stashed atlas texture from a previous flush.
    sk_sp<GrRenderTargetContext> makeRenderTargetContext(GrOnFlushResourceProvider*,
                                                         sk_sp<GrTexture> backingTexture = nullptr);

private:
    class Node;

    bool internalPlaceRect(int w, int h, SkIPoint16* loc);

    const int fMaxTextureSize;
    int fWidth, fHeight;
    std::unique_ptr<Node> fTopNode;
    SkISize fDrawBounds = {0, 0};

    int fUserBatchID;

    // Not every atlas will have a unique key -- a mainline CCPR one won't if we don't stash any
    // paths, and only the first atlas in the stack is eligible to be stashed.
    GrUniqueKey fUniqueKey;

    sk_sp<CachedAtlasInfo> fCachedAtlasInfo;
    sk_sp<GrTextureProxy> fTextureProxy;
    sk_sp<GrTexture> fBackingTexture;
};

/**
 * This class implements an unbounded stack of atlases. When the current atlas reaches the
 * implementation-dependent max texture size, a new one is pushed to the back and we continue on.
 */
class GrCCAtlasStack {
public:
    GrCCAtlasStack(GrPixelConfig pixelConfig, const GrCCAtlas::Specs& specs, const GrCaps* caps)
            : fPixelConfig(pixelConfig), fSpecs(specs), fCaps(caps) {}

    bool empty() const { return fAtlases.empty(); }
    const GrCCAtlas& front() const { SkASSERT(!this->empty()); return fAtlases.front(); }
    GrCCAtlas& front() { SkASSERT(!this->empty()); return fAtlases.front(); }
    GrCCAtlas& current() { SkASSERT(!this->empty()); return fAtlases.back(); }

    class Iter {
    public:
        Iter(GrCCAtlasStack& stack) : fImpl(&stack.fAtlases) {}
        bool next() { return fImpl.next(); }
        GrCCAtlas* operator->() const { return fImpl.get(); }
    private:
        typename GrTAllocator<GrCCAtlas>::Iter fImpl;
    };

    // Adds a rect to the current atlas and returns the offset from device space to atlas space.
    // Call current() to get the atlas it was added to.
    //
    // If the return value is non-null, it means the given rect did not fit in the then-current
    // atlas, so it was retired and a new one was added to the stack. The return value is the
    // newly-retired atlas. The caller should call setUserBatchID() on the retired atlas before
    // moving on.
    GrCCAtlas* addRect(const SkIRect& devIBounds, SkIVector* devToAtlasOffset);

private:
    const GrPixelConfig fPixelConfig;
    const GrCCAtlas::Specs fSpecs;
    const GrCaps* const fCaps;
    GrSTAllocator<4, GrCCAtlas> fAtlases;
};

inline void GrCCAtlas::Specs::accountForSpace(int width, int height) {
    fMinWidth = SkTMax(width, fMinWidth);
    fMinHeight = SkTMax(height, fMinHeight);
    fApproxNumPixels += (width + kPadding) * (height + kPadding);
}

#endif
