//
// RT64
//

#include "rt64_gbi_s2dex.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../include/rt64_extended_gbi.h"

#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_f3dex.h"
#include "rt64_gbi_f3dex2.h"
#include "rt64_gbi_rdp.h"

//#define LOG_BGRECT_METHODS
//#define LOG_LOADTXTR_METHODS

namespace RT64 {
    namespace GBI_S2DEX {
        void drawObjSprite(State *state, const uObjSprite &sprite);
        void drawObjRectangle(State *state, const uObjSprite &sprite);
        void drawObjRectangleR(State *state, const uObjSprite &sprite);

        void objRenderMode(State *state, DisplayList **dl) {
            state->rsp->setObjRenderMode((*dl)->w1);
        }

        void moveWord(State *state, DisplayList **dl) {
            switch ((*dl)->p0(0, 8)) {
            case G_MW_GENSTAT:
                assert(false);
                break;
            default:
                GBI_F3D::moveWord(state, dl);
                break;
            }
        }

        void rdpHalf0(State *state, DisplayList **dl) {
            uint8_t nextCode = (*dl + 1)->w0 >> 24;
            if (nextCode == S2DEX_G_SELECT_DL) {
                assert(false);
            }
            else if (nextCode == F3D_G_RDPHALF_1) {
                GBI_RDP::texrect(state, dl);
            }
        }

        void bg1CycTMEMLoadTile(RDP &rdp, const uObjScaleBg_t &scaleBg, uint32_t imagePtr, uint16_t imageSrcWsize, int16_t loadLines, int16_t tmemSH) {
            // TODO: Does it make sense that when using 32-bit images, the lrt is 0?
            const bool is32Bits = (scaleBg.imageSiz == G_IM_SIZ_32b);
            uint8_t textureImageSiz = is32Bits ? G_IM_SIZ_32b : G_IM_SIZ_16b;
            uint16_t tileLrt = is32Bits ? 0 : ((loadLines << 2) - 1);
            rdp.setTextureImage(G_IM_FMT_RGBA, textureImageSiz, imageSrcWsize >> 1, imagePtr);
            rdp.loadTile(G_TX_LOADTILE, 0, 0, (tmemSH - 1) << 4, tileLrt);
        }
        
        void bg1CycTMEMSetAndLoadTile(RDP &rdp, const uObjScaleBg_t &scaleBg, uint32_t imagePtr, uint16_t imageSrcWsize, int16_t loadLines, uint16_t tmemSliceWmax, int16_t tmemAdrs, int16_t tmemSH) {
            const bool is32Bits = (scaleBg.imageSiz == G_IM_SIZ_32b);
            uint8_t setSiz = is32Bits ? G_IM_SIZ_32b : G_IM_SIZ_16b;
            rdp.setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, setSiz, tmemSliceWmax, tmemAdrs, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            bg1CycTMEMLoadTile(rdp, scaleBg, imagePtr, imageSrcWsize, loadLines, tmemSH);
        }

        void bg1CycTMEMLoad(RDP &rdp, const uObjScaleBg_t &scaleBg, uint32_t imageTop, uint32_t &imagePtr, int16_t &imageRemain, uint16_t imageSrcWsize, uint16_t imagePtrX0, 
            int16_t tmemSrcLines, uint16_t tmemSliceWmax, int16_t drawLines, int16_t usesBilerp, int16_t flagSplit)
        {
            int16_t loadLines = drawLines + usesBilerp;
            int16_t iLoadable = imageRemain - flagSplit;

            // Load everything at once.
            if (iLoadable >= loadLines) {
                bg1CycTMEMLoadTile(rdp, scaleBg, imagePtr, imageSrcWsize, loadLines, tmemSliceWmax);
                imagePtr += imageSrcWsize * drawLines;
                imageRemain -= drawLines;
            }
            // Partitioned load.
            else {
                uint32_t imageTopSeg = imageTop & 0xFF000000;
                int16_t subSliceY2 = imageRemain;
                int16_t subSliceL2 = loadLines - subSliceY2;
                int16_t subSliceD2 = drawLines - subSliceY2;
                if (subSliceL2 > 0) {
                    uint32_t imagePtr2 = imageTop + imagePtrX0;
                    if (subSliceY2 & 0x1) {
                        imagePtr2 -= imageSrcWsize;
                        imagePtr2 = imageTopSeg | (imagePtr2 & 0x00FFFFFF);
                        subSliceY2--;
                        subSliceL2++;
                    }

                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr2, imageSrcWsize, subSliceL2, tmemSliceWmax, subSliceY2 * tmemSliceWmax, tmemSliceWmax);
                }

                if (flagSplit) {
                    uint32_t imagePtr1A = imagePtr + iLoadable * imageSrcWsize;
                    uint32_t imagePtr1B = imageTop;
                    int16_t subSliceY1 = iLoadable;
                    int16_t subSliceL1 = iLoadable & 1;
                    if (subSliceL1) {
                        imagePtr1A -= imageSrcWsize;
                        imagePtr1B -= imageSrcWsize;
                        imagePtr1B = imageTopSeg | (imagePtr1B & 0x00FFFFFF);
                        subSliceY1--;
                    }

                    subSliceL1++;

                    int16_t tmemSH_A = (imageSrcWsize - imagePtrX0) >> 3;
                    int16_t tmemSH_B = tmemSliceWmax - tmemSH_A;
                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr1B, imageSrcWsize, subSliceL1, tmemSliceWmax, subSliceY1 * tmemSliceWmax + tmemSH_A, tmemSH_B);
                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr1A, imageSrcWsize, subSliceL1, tmemSliceWmax, subSliceY1 * tmemSliceWmax, tmemSH_A);
                }

                if (iLoadable > 0) {
                    bg1CycTMEMSetAndLoadTile(rdp, scaleBg, imagePtr, imageSrcWsize, iLoadable, tmemSliceWmax, 0, tmemSliceWmax);
                }
                else {
                    uint8_t loadSiz = (scaleBg.imageSiz == G_IM_SIZ_32b) ? G_IM_SIZ_32b : G_IM_SIZ_16b;
                    rdp.setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, loadSiz, tmemSliceWmax, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                }

                imageRemain -= drawLines;
                if (imageRemain > 0) {
                    imagePtr += imageSrcWsize * drawLines;
                }
                else {
                    imageRemain = tmemSrcLines - subSliceD2;
                    imagePtr = imageTop + subSliceD2 * imageSrcWsize + imagePtrX0;
                }
            }
        }

        void bg1Cyc(State *state, DisplayList **dl) {
#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bg1Cyc::start(0x%08X)", (*dl)->w1);
#       endif

            RDP *rdp = state->rdp.get();
            RSP *rsp = state->rsp.get();
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            // TODO load this into the S2D struct buffer for a more accurate implementation in case there's ever command state bleed.
            const uObjBg *bgObject = reinterpret_cast<const uObjBg *>(state->fromRDRAM(rdramAddress));
            const uObjBg_t &bg = bgObject->bg;
            const uObjScaleBg_t &scaleBg = bgObject->scaleBg;

            // Validate the image loading method is one of the two supported methods. It's worth noting 
            // that despite the load block method being documented, it doesn't actually work in the retail
            // microcode and it should therefore be treated as load tile as well.
            assert((bg.imageLoad == S2DEX_G_BGLT_LOADBLOCK) || (bg.imageLoad == S2DEX_G_BGLT_LOADTILE));

            // The scale should at least be 1.
            uint16_t scaleW = std::max(scaleBg.scaleW, uint16_t(1));
            uint16_t scaleH = std::max(scaleBg.scaleH, uint16_t(1));

            // Max frame area determined from the image size and the scale.
            int32_t frameWmax = (int32_t(scaleBg.imageW) << 10) / scaleW;
            int32_t frameHmax = (int32_t(scaleBg.imageH) << 10) / scaleH;
            frameWmax = (frameWmax - 1) & ~0x3;
            frameHmax = (frameHmax - 1) & ~0x3;

            // Clamp the frame's width to the max frame area.
            int16_t frameW = scaleBg.frameW;
            int16_t frameH = scaleBg.frameH;
            frameWmax = std::max(scaleBg.frameW - frameWmax, 0);
            frameHmax = std::max(scaleBg.frameH - frameHmax, 0);
            frameW -= frameWmax;
            frameH -= frameHmax;
            
            int16_t frameX0 = scaleBg.frameX;
            int16_t frameY0 = scaleBg.frameY;
            const bool imageFlipS = scaleBg.imageFlip & S2DEX_G_BG_FLAG_FLIPS;
            if (imageFlipS) {
                frameX0 += frameWmax;
            }

            // TODO: This scissor state probably needs to be pulled from the one tracked
            // by the RSP instead of the RDP, which is not currently implemented yet.
            const FixedRect &scissorRect = rdp->scissorRectStack[rdp->scissorStackSize - 1];
            int32_t scissorX0 = scissorRect.ulx;
            int32_t scissorY0 = scissorRect.uly;
            int32_t scissorX1 = scissorRect.lrx;
            int32_t scissorY1 = scissorRect.lry;
            int16_t pixX0 = int16_t(std::max(scissorX0 - frameX0, 0));
            int16_t pixY0 = int16_t(std::max(scissorY0 - frameY0, 0));
            int16_t pixX1 = int16_t(std::max(frameW - scissorX1 + frameX0, 0));
            int16_t pixY1 = int16_t(std::max(frameH - scissorY1 + frameY0, 0));

            // Cut out the part outside of the current scissor.
            frameW = frameW - (pixX0 + pixX1);
            frameH = frameH - (pixY0 + pixY1);
            frameX0 = frameX0 + pixX0;
            frameY0 = frameY0 + pixY0;

            // The frame is no longer valid, don't draw anything.
            if ((frameW <= 0) || (frameH <= 0)) {
                return;
            }

            // Compute the frame size after being clamped by the scissor.
            int16_t frameX1 = frameX0 + frameW;
            int16_t framePtrY0 = frameY0 >> 2;
            int16_t frameRemain = frameH >> 2;
            int16_t imageSrcW = scaleBg.imageW << 3;
            int16_t imageSrcH = scaleBg.imageH << 3;
            
            // Find the corresponding range in the image for the frame.
            // The image size needs to be extended when using bilerp.
            int16_t usesBilerp = (rsp->objRenderMode & S2DEX_G_OBJRM_BILERP) ? 1 : 0;

            // Some games will incorrectly set this flag despite not actually using bilerp during drawing. The side effect is that it'll end
            // up using a method of loading textures that makes it almost impossible for the emulation to detect tiles that can be upscaled due
            // to the various shenanigans the microcode has to do to load an image that is bigger than what it really is.
            // 
            // This enhancement falls under the category of a developer intended fix because there's a clear mismatch between the end result
            // and the loading method used to achieve it, as evidenced by the current state of the RDP. The end result is it makes the loading
            // logic much simpler and allows the emulation to upscale the tiles properly.
            //
            const bool rdpUsesBilerp = (rdp->otherMode.textFilt() == G_TF_BILERP);
            const bool bilerpFixEnabled = state->ext.enhancementConfig->s2dex.fixBilerpMismatch;
            if (usesBilerp && !rdpUsesBilerp && bilerpFixEnabled) {
                usesBilerp = 0;
            }

            int16_t imageW = (frameW * scaleW) >> 7;
            int16_t imageSliceW = imageW + (usesBilerp * 32);
            int16_t imageX0;
            if (imageFlipS) {
                imageX0 = scaleBg.imageX + ((pixX1 * scaleW) >> 7);
            }
            else {
                imageX0 = scaleBg.imageX + ((pixX0 * scaleW) >> 7);
            }

            int16_t imageY0 = scaleBg.imageY + (pixY0 * scaleH >> 7);
            int32_t imageYorig = scaleBg.imageYorig;

            // Keep scrolling down the image one row at a time if the 
            // left of the image is greater than the source's width.
            while (imageX0 >= imageSrcW) {
                imageX0 -= imageSrcW;
                imageY0 += 32;
                imageYorig += 32;
            }

            // Loop around when the top of the image is greater than
            // the source's height.
            while (imageY0 >= imageSrcH) {
                imageY0 -= imageSrcH;
                imageYorig -= imageSrcH;
            }

            // The TMEM loads will need to be split if the image range covers the entire image's width.
            int16_t flagSplit = (imageX0 + imageSliceW >= imageSrcW);

            // How many lines can be loaded into TMEM per draw.
            int16_t tmemSrcLines = imageSrcH >> 5;

            // Determine the amount of TMEM that can be used based on the format and size of the image.
            // We limit the amount of TMEM that can be used to half for CI images since the upper region
            // needs to be used for the palette.
            int16_t tmemSize = (scaleBg.imageFmt == G_IM_FMT_CI) ? 256 : 512;
            int16_t tmemShift = (0x200 >> scaleBg.imageSiz);
            int16_t tmemMask = (tmemShift - 1);
            int32_t imageSliceWmax;
            if (scaleBg.imageSiz == G_IM_SIZ_32b) {
                tmemSize = 480;
                imageSliceWmax = 0x2800;
            }
            else {
                imageSliceWmax = (scaleBg.frameW * scaleW) >> 7;
                imageSliceWmax = std::min(imageSliceWmax + (usesBilerp << 5), int32_t(imageSrcW));
            }

            // Get the amount of lines that can be loaded into TMEM at once.
            uint16_t tmemSliceWmax = (imageSliceWmax + tmemMask) / tmemShift + 1;
            int16_t tmemSliceLines = tmemSize / tmemSliceWmax;
            int16_t imageSliceLines = tmemSliceLines - usesBilerp;
            int32_t frameSliceLines = (imageSliceLines << 20) / scaleH;

            // Figure out which line to start with from the image coordinates.
            int32_t imageLYoffset = (int32_t(imageY0) - imageYorig) << 5;
            if (imageLYoffset < 0) {
                imageLYoffset -= (scaleH - 1);
            }

            int32_t frameLYoffset = (imageLYoffset / scaleH) << 10;

            // Determine which slice number corresponds to this offset.
            int32_t imageNumSlice;
            if (frameLYoffset >= 0) {
                imageNumSlice = frameLYoffset / frameSliceLines;
            }
            else {
                imageNumSlice = (frameLYoffset - frameSliceLines + 1) / frameSliceLines;
            }

            // How much of the first drawn rectangle will be hidden at the top of the frame.
            int32_t frameLSliceL0 = frameSliceLines * imageNumSlice;
            int32_t frameLYslice = frameLSliceL0 & ~0x3FF;
            int32_t frameLHidden = frameLYoffset - frameLYslice;
            int32_t imageLHidden = (frameLHidden >> 10) * scaleH;
            frameLSliceL0 = (frameLSliceL0 & 0x3FF) + frameSliceLines - frameLHidden;

            // Image parameters for the slice.
            uint16_t imageT = (imageLHidden >> 5) & 0x1F;
            imageLHidden >>= 10;
            int16_t imageISliceL0 = imageSliceLines - imageLHidden;
            int16_t imageIY0 = imageSliceLines * imageNumSlice + (imageYorig & ~0x1F) / 32 + imageLHidden;
            uint16_t imageHLowered = (scaleBg.imageH >> 2);
            if (imageIY0 < 0) {
                imageIY0 += imageHLowered;
            }

            if (imageIY0 >= imageHLowered) {
                imageIY0 -= imageHLowered;
            }

            const uint32_t imageAddress = state->rsp->fromSegmented(scaleBg.imageAddress);
            uint16_t imageSrcWsize = (imageSrcW / tmemShift) << 3;
            uint16_t imagePtrX0 = (imageX0 / tmemShift) << 3;
            uint32_t imagePtr = imageAddress + imageSrcWsize * imageIY0 + imagePtrX0;
            uint16_t imageS = imageX0 & tmemMask;
            if (imageFlipS) {
                imageS = -(imageS + imageW);
            }

            // RDP Commands constant throughout the image.
            uint8_t loadSiz = (scaleBg.imageSiz == G_IM_SIZ_32b) ? G_IM_SIZ_32b : G_IM_SIZ_16b;
            rdp->setTile(G_TX_LOADTILE, G_IM_FMT_RGBA, loadSiz, tmemSliceWmax, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            rdp->setTile(G_TX_RENDERTILE, scaleBg.imageFmt, scaleBg.imageSiz, tmemSliceWmax, 0, uint8_t(scaleBg.imagePal), G_TX_WRAP | G_TX_MIRROR, G_TX_WRAP | G_TX_MIRROR, 0xF, 0xF, 0, 0);

            // Attempt to use a single tile instead to draw the entire rect at once if there's a framebuffer copy available.
            // If it fails to find a possible tile copy, fall back to the regular approach.
            // FIXME: The rest of the code should still run to account for state bleeding without drawing any rectangles.
            if (state->ext.enhancementConfig->s2dex.framebufferFastPath) {
                int16_t uls = imageS;
                int16_t ult = imageT;
                int16_t lrs = uls + scaleBg.imageW;
                int16_t lrt = ult + scaleBg.imageH;
                rdp->setTextureImage(G_IM_FMT_RGBA, loadSiz, imageSrcWsize >> 1, imagePtr);
                rdp->setTileSize(G_TX_RENDERTILE, uls, ult, lrs, lrt);

                uint64_t replacementHash = 0;
                bool replacementCheck = rdp->loadTileReplacementCheck(G_TX_LOADTILE, uls, ult, lrs, lrt, scaleBg.imageSiz, scaleBg.imageFmt, scaleBg.imageLoad, scaleBg.imagePal, replacementHash);
                bool singleTileMode = replacementCheck || rdp->loadTileCopyCheck(G_TX_LOADTILE, uls, ult, lrs, lrt);
                state->startSpriteCommand(replacementHash);
                if (singleTileMode) {
                    int32_t uly = frameY0 & ~0x3;
                    rdp->setTileReplacementHash(G_TX_RENDERTILE, replacementHash);
                    rdp->loadTile(G_TX_LOADTILE, uls, ult, lrs, lrt);
                    rdp->drawTexRect(frameX0, uly, frameX1, uly + frameH, G_TX_RENDERTILE, uls, ult, scaleW, scaleH, false);
                    rdp->clearTileReplacementHash(G_TX_RENDERTILE);
                    state->endSpriteCommand();
                    return;
                }
            }

            rdp->setTileSize(G_TX_RENDERTILE, 0, 0, 0, 0);
            
            // Draw the image.
            int16_t imageRemain = tmemSrcLines - imageIY0;
            int16_t imageSliceH = imageISliceL0;
            int32_t frameSliceCount = frameLSliceL0;
            while (frameRemain > 0) {
                int16_t frameSliceH = frameSliceCount >> 10;
                if (frameSliceH <= 0) {
                    imageRemain -= imageSliceH;
                    if (imageRemain > 0) {
                        imagePtr += imageSrcWsize * imageSliceH;
                    }
                    else {
                        imagePtr = imageAddress - (imageRemain * imageSrcWsize) + imagePtrX0;
                        imageRemain += tmemSrcLines;
                    }
                }
                else {
                    frameSliceCount &= 0x3FF;
                    frameRemain -= frameSliceH;

                    // Final slice.
                    if (frameRemain < 0) {
                        frameSliceH += frameRemain;
                        imageSliceH += ((frameRemain * scaleH) >> 10) + 1;
                        imageSliceH = std::min(imageSliceH, imageSliceLines);
                    }

                    bg1CycTMEMLoad(*rdp, scaleBg, imageAddress, imagePtr, imageRemain, imageSrcWsize, imagePtrX0, tmemSrcLines, tmemSliceWmax, imageSliceH, usesBilerp, flagSplit);

                    int16_t framePtrY1 = framePtrY0 + frameSliceH;
                    rdp->drawTexRect(frameX0, framePtrY0 << 2, frameX1, framePtrY1 << 2, G_TX_RENDERTILE, imageS, imageT, scaleW, scaleH, false);
                    framePtrY0 = framePtrY1;
                }

                frameSliceCount += frameSliceLines;
                imageSliceH = imageSliceLines;
                imageT = 0;
            }

            state->endSpriteCommand();

#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bg1Cyc::end(0x%08X)", (*dl)->w1);
#       endif
        }
        
        void bgCopy(State *state, DisplayList **dl) {
            // TODO: Reimplement more accurately to match the microcode.

#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bgCopy::start(0x%08X)", (*dl)->w1);
#       endif
            
            RDP *rdp = state->rdp.get();
            RSP* rsp = state->rsp.get();
            const uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1);
            // TODO load this into the S2D struct buffer for a more accurate implementation in case there's ever command state bleed.
            const uObjBg *bgObject = reinterpret_cast<const uObjBg *>(state->fromRDRAM(rdramAddress));
            const uObjBg_t &bg = bgObject->bg;
            assert(bg.imageLoad == S2DEX_G_BGLT_LOADTILE); // TODO: Only the load tile version is implemented.
            
            const uint16_t TMEMAddress = 0;
            const uint8_t lineShiftOffset = (bg.imageSiz == G_IM_SIZ_32b) ? 1 : 0;
            const uint16_t bgLine = bg.imageW >> (2 + bg.imageSiz - lineShiftOffset);
            const uint16_t dsdx = 4 << 10;
            const uint16_t dsdy = 4 << 8;
            const uint16_t lrSubstract = 4;
            const uint16_t bgRectLrs = bg.imageW - lrSubstract;
            const uint32_t savedOtherModeH = rdp->otherMode.H;
            rsp->setTextureImage(bg.imageFmt, bg.imageSiz, bg.imageW >> 2, state->rsp->fromSegmented(bg.imageAddress));
            rdp->setTile(G_TX_LOADTILE, bg.imageFmt, bg.imageSiz, bgLine, TMEMAddress, static_cast<uint8_t>(bg.imagePal), 0, 0, 0, 0, 0, 0);
            rdp->setTile(G_TX_RENDERTILE, bg.imageFmt, bg.imageSiz, bgLine, TMEMAddress, static_cast<uint8_t>(bg.imagePal), 0, 0, 0, 0, 0, 0);

            // Attempt to use a single tile instead to draw the entire rect at once if there's a framebuffer copy available.
            // If it fails to find a possible tile copy, fall back to the regular approach.
            // FIXME: The rest of the code should still run to account for state bleeding without drawing any rectangles.
            uint16_t bgRectLrt = bg.imageH - lrSubstract;
            uint64_t replacementHash = 0;
            if (state->ext.enhancementConfig->s2dex.framebufferFastPath) {
                bool replacementCheck = rdp->loadTileReplacementCheck(G_TX_LOADTILE, 0, 0, bgRectLrs, bgRectLrt, bg.imageSiz, bg.imageFmt, bg.imageLoad, bg.imagePal, replacementHash);
                bool singleTileMode = replacementCheck || rdp->loadTileCopyCheck(G_TX_LOADTILE, 0, 0, bgRectLrs, bgRectLrt);
                state->startSpriteCommand(replacementHash);
                if (singleTileMode) {
                    rdp->setTileReplacementHash(G_TX_RENDERTILE, replacementHash);
                    rdp->setTileSize(G_TX_RENDERTILE, 0, 0, bgRectLrs, bgRectLrt);
                    rdp->loadTile(G_TX_LOADTILE, 0, 0, bgRectLrs, bgRectLrt);
                    rdp->drawTexRect(bg.frameX, bg.frameY, bg.frameX + bg.frameW - lrSubstract, bg.frameY + bg.frameH - lrSubstract, G_TX_RENDERTILE, 0, 0, dsdx, dsdy, false);
                    rdp->clearTileReplacementHash(G_TX_RENDERTILE);
                    state->endSpriteCommand();
                    return;
                }
            }

            const uint32_t Division = (bg.imageFmt == G_IM_FMT_CI) ? 2 : 1;
            const uint16_t bgRectCount = bg.imageH / (bg.tmemH / Division); // TODO: Check for remaining pixels for the last Texrect.
            bgRectLrt = (bg.tmemH / Division) - lrSubstract;
            rdp->setTileSize(G_TX_RENDERTILE, 0, 0, bgRectLrs, bgRectLrt);

            for (uint16_t r = 0; r < bgRectCount; r++) {
                const uint16_t bgRectUlt = r * (bg.tmemH / Division);
                rdp->loadTile(G_TX_LOADTILE, 0, bgRectUlt, bgRectLrs, bgRectUlt + bgRectLrt);
                rdp->drawTexRect(bg.frameX, bg.frameY + bgRectUlt, bg.frameX + bg.frameW - lrSubstract, bg.frameY + bgRectUlt + bgRectLrt, G_TX_RENDERTILE, 0, 0, dsdx, dsdy, false);
            }

            state->endSpriteCommand();

#       ifdef LOG_BGRECT_METHODS
            RT64_LOG_PRINTF("bgCopy::end(0x%08X)", (*dl)->w1);
#       endif
        }

        uint32_t extractBits(uint32_t word, uint8_t pos, uint8_t bits) {
            return ((word >> pos) & ((0x01 << bits) - 1));
        }

        void readS2DStruct(State *state, uint32_t ptr, uint32_t loadSize) {
            // Convert the segmented obj pointer
            uint32_t rdramAddress = state->rsp->fromSegmentedMasked(ptr);
            // Mask the address as the RSP DMA hardware would
            rdramAddress &= RSP_DMA_MASK;
            // Truncate the load size as the ucode does
            loadSize &= 0xFF;
            // Load the struct from RDRAM into the S2D struct buffer
            memcpy(state->rsp->S2D.struct_buffer.data(), state->fromRDRAM(rdramAddress), loadSize);
        }

        void doLoadTxtr(State *state, const uObjTxtr *obj) {
            // Not sure what this does yet so it's just left as the original register name.
            // Maybe used for tracking what the most recent command was so it can insert syncs as needed?
            int32_t r1 = state->rsp->S2D.data_02AE;

            uint32_t sid = (uint8_t)(obj->sid);
            // Must be divisible by 4 and no more than 12 to work correctly.
            // Technically the RSP does support unaligned reads/writes, so perfect simulation would require ditching the divisibility
            // requirement and handling those cases correctly. Handling values above 12 is basically impossible because it would
            // depend on dmem overrun.
            assert((sid & 0b11) == 0 && sid <= 12);

            uint32_t mask = obj->mask;
            uint32_t flag = obj->flag;
            // Get the status for the given id.
            uint32_t status = state->rsp->S2D.statuses[sid / 4];

            // Skip the load if the masked status is equal to the provided flag.
            if ((status & mask) != flag) {
                // Update the status for the given id.
                state->rsp->S2D.statuses[sid / 4] = (status & ~mask) | (flag & mask);
                state->rsp->S2D.data_02AE = -127;

                if (r1 == -127) {
                    // RDP Tile Sync, not needed
                }

                uint32_t tsize = obj->val1;
                state->rdp->setTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, tsize + 1, state->rsp->fromSegmented(obj->image));

                // The most accurate way to implement this is to combine the calculated fields as the RSP does and then decode them as the RDP would.
                uint32_t type_mask = (int8_t)(obj->type >> 16); // The cast to signed is very important here, as sign extension is required.
                uint32_t w0_part1 = ((tsize + 1) & type_mask) << 7;
                uint32_t w0_part2 = ((uint8_t)(obj->type >> 8)) << 16;
                uint32_t settile_w0 = obj->tmem | w0_part1 | w0_part2; // combined settile w0
                uint32_t fmt = extractBits(settile_w0, 21, 3);
                uint32_t siz = extractBits(settile_w0, 19, 2);
                uint32_t line = extractBits(settile_w0, 9, 9);
                uint32_t tmem = extractBits(settile_w0, 0, 9);

                state->rdp->setTile(G_TX_LOADTILE, fmt, siz, line, tmem, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

                if (r1 >= 0) {
                    // RDP Load Sync, not needed
                }

                // Same deal as above, but we need to decode differently depending on the command.
                uint32_t load_w1 = ((tsize << 14) | obj->val2) & 0x00FFFFFF;
                // These command ids are masked by 0x3F because the RDP ignores the top two bits.
                uint32_t load_command_id = obj->type & 0x3F;
                if (load_command_id == (G_LOADBLOCK & 0x3F)) {
                    uint32_t lrs = extractBits(load_w1, 12, 12);
                    uint32_t dxt = extractBits(load_w1, 0, 12);
                    state->rdp->loadBlock(G_TX_LOADTILE, 0, 0, lrs, dxt);
                } else if (load_command_id == (G_LOADTILE & 0x3F)) {
                    uint32_t lrs = extractBits(load_w1, 12, 12);
                    uint32_t lrt = extractBits(load_w1, 0, 12);
                    state->rdp->loadTile(G_TX_LOADTILE, 0, 0, lrs, lrt);
                } else if (load_command_id == (G_LOADTLUT & 0x3F)) {
                    uint32_t lrs = extractBits(load_w1, 12, 12);
                    state->rdp->loadTLUT(G_TX_LOADTILE, 0, 0, lrs, 0);
                } else {
                    assert(false && "Invalid sprite load command");
                }
            }
        }

        void objLoadTxtr(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxtr::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);
            
            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxtr::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void objLoadTxSprite(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxSprite::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);
            
            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
            
            // Execute the sprite draw
            uObjSprite sprite;
            memcpy(&sprite, state->rsp->S2D.struct_buffer.data() + sizeof(uObjTxtr), sizeof(sprite));
            drawObjSprite(state, sprite);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxSprite::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void objLoadTxRect(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRect::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);
            
            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
            
            // Execute the rect draw
            uObjSprite sprite;
            memcpy(&sprite, state->rsp->S2D.struct_buffer.data() + sizeof(uObjTxtr), sizeof(sprite));
            drawObjRectangle(state, sprite);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRect::end(0x%08X)", (*dl)->w1);
        #endif
        }

        void objLoadTxRectR(State *state, DisplayList **dl) {
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRectR::start(0x%08X)", (*dl)->w1);
        #endif
            // Load the struct from rdram
            readS2DStruct(state, (*dl)->w1, ((*dl)->w0 & 0xFFFFFF) + 1);

            // Execute the texture load
            const uObjTxSprite* obj = (uObjTxSprite*)state->rsp->S2D.struct_buffer.data();
            doLoadTxtr(state, &obj->txtr);
            
            // Execute the rect r draw
            uObjSprite sprite;
            memcpy(&sprite, state->rsp->S2D.struct_buffer.data() + sizeof(uObjTxtr), sizeof(sprite));
            drawObjRectangleR(state, sprite);
        #ifdef LOG_LOADTXTR_METHODS
            RT64_LOG_PRINTF("objLoadTxRectR::end(0x%08X)", (*dl)->w1);
        #endif
        }

        // Coordinate correctors the microcode applies before drawing a sprite.
        // Selected by the object render mode: the shrink/widen bits pick the
        // texture/rect insets and the bilerp bit picks the subpixel mask.
        struct SpriteCorrector {
            int16_t A0, A1, A2, A3, B0, B2, B3;
        };

        static SpriteCorrector spriteCorrector(uint32_t objRenderMode) {
            // Flattened to s16 in the order the microcode indexes them.
            static const int16_t A01[] = {
                0x0000, 0x0000, 0x0020, 0x0010, 0x0040, 0x0020, 0x0060, 0x0030,
                -12,    0x0000, 0x0014, 0x0010, 0x0034, 0x0020, 0x0054, 0x0030
            };
            static const int16_t A23[] = {
                -2, 0x0001, -2, -2, 0x0000, 0x0001, 0x0000, 0x0000
            };
            static const int16_t B03[] = {
                0x0000, -4, 0x0001, 0x0000, 0x0003, -1, 0x0000, -16
            };

            const uint32_t o1 = (objRenderMode & (S2DEX_G_OBJRM_SHRINKSIZE_1 | S2DEX_G_OBJRM_SHRINKSIZE_2 | S2DEX_G_OBJRM_WIDEN)) >> 3;
            const uint32_t o2 = (objRenderMode & (S2DEX_G_OBJRM_SHRINKSIZE_1 | S2DEX_G_OBJRM_BILERP)) >> 2;
            const uint32_t o3 = (objRenderMode & S2DEX_G_OBJRM_BILERP) >> 1;

            SpriteCorrector cc;
            cc.A0 = A01[(0 + o1) ^ 1];
            cc.A1 = A01[(1 + o1) ^ 1];
            cc.A2 = A23[(0 + o2) ^ 1];
            cc.A3 = A23[(1 + o2) ^ 1];
            cc.B0 = B03[(0 + o3) ^ 1];
            cc.B2 = B03[(2 + o3) ^ 1];
            cc.B3 = B03[(3 + o3) ^ 1];
            return cc;
        }

        static void readObjSprite(State *state, uint32_t ptr, uObjSprite &sprite) {
            uint32_t rdramAddress = state->rsp->fromSegmentedMasked(ptr) & RSP_DMA_MASK;
            memcpy(&sprite, state->fromRDRAM(rdramAddress), sizeof(sprite));
        }

        // The sprite's texture is already in TMEM (loaded by the preceding
        // G_OBJ_LOADTXTR); this sets up the render tile the way the ucode does.
        static void setSpriteTile(State *state, const uObjSprite &sprite) {
            const uint32_t w = std::max<uint32_t>(sprite.imageW >> 5, 1);
            const uint32_t h = std::max<uint32_t>(sprite.imageH >> 5, 1);
            state->rdp->setTile(G_TX_RENDERTILE, sprite.imageFmt, sprite.imageSiz, sprite.imageStride,
                sprite.imageAdrs, sprite.imagePal, G_TX_CLAMP | G_TX_NOMIRROR, G_TX_CLAMP | G_TX_NOMIRROR,
                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            state->rdp->setTileSize(G_TX_RENDERTILE, 0, 0, (w - 1) << 2, (h - 1) << 2);
        }

        // One textured rectangle: screen coords in the RDP's 1/4 pixel units,
        // texture coords in 1/32 texel units.
        static void drawSpriteRectStep(State *state, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
            int32_t s0, int32_t t0, int32_t dsdx, int32_t dtdy)
        {
            if (x1 == x0 || y1 == y0) {
                return;
            }

            state->rdp->drawTexRect(x0, y0, x1, y1, G_TX_RENDERTILE, static_cast<int16_t>(s0), static_cast<int16_t>(t0),
                static_cast<int16_t>(dsdx), static_cast<int16_t>(dtdy), false);
            state->endSpriteCommand();
        }

        static void drawSpriteRect(State *state, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
            int32_t s0, int32_t t0, int32_t s1, int32_t t1)
        {
            if (x1 == x0 || y1 == y0) {
                return;
            }

            const int32_t dsdx = ((s1 - s0) << 7) / (x1 - x0);
            const int32_t dtdy = ((t1 - t0) << 7) / (y1 - y0);
            drawSpriteRectStep(state, x0, y0, x1, y1, s0, t0, dsdx, dtdy);
        }

        // A rotated or sheared sprite: the microcode emits the matrix-mapped
        // quad as two RDP triangles, texture stretched corner to corner like
        // the axis-aligned case below. Corners are in 1/4 pixels, in the order
        // upper-left, upper-right, lower-left, lower-right of the image.
        static void drawRotatedSprite(State *state, const uObjSprite &sprite, const int32_t px[4], const int32_t py[4]) {
            float s0 = 0.0f;
            float t0 = 0.0f;
            float s1 = std::max<int32_t>(sprite.imageW - 32, 0) / 32.0f;
            float t1 = std::max<int32_t>(sprite.imageH - 32, 0) / 32.0f;
            if (sprite.imageFlags & S2DEX_G_BG_FLAG_FLIPS) {
                std::swap(s0, s1);
            }
            if (sprite.imageFlags & S2DEX_G_BG_FLAG_FLIPT) {
                std::swap(t0, t1);
            }

            const float cs[4] = { s0, s1, s0, s1 };
            const float ct[4] = { t0, t0, t1, t1 };
            const int order[6] = { 0, 1, 2, 1, 3, 2 };
            float pos[6 * 4];
            float tc[6 * 2];
            float col[6 * 4] = {};
            for (int i = 0; i < 6; i++) {
                const int v = order[i];
                pos[i * 4 + 0] = px[v] / 4.0f;
                pos[i * 4 + 1] = py[v] / 4.0f;
                pos[i * 4 + 2] = 0.0f;
                pos[i * 4 + 3] = 1.0f;
                // The raw triangle path takes texture coordinates in half
                // texels, the scale RT64's own RDP triangle decoder produces
                // (S/T in s10.5 times 1024 / 16384).
                tc[i * 2 + 0] = cs[v] * 2.0f;
                tc[i * 2 + 1] = ct[v] * 2.0f;
            }

            state->rdp->drawTris(2, pos, tc, col, G_TX_RENDERTILE, 0);
            state->endSpriteCommand();
        }

        void drawObjSprite(State *state, const uObjSprite &sprite) {
            setSpriteTile(state, sprite);

            const SpriteCorrector cc = spriteCorrector(state->rsp->objRenderMode);
            const int32_t scaleW = std::max<int32_t>(sprite.scaleW, 1);
            const int32_t scaleH = std::max<int32_t>(sprite.scaleH, 1);

            const int32_t x0 = (state->rsp->S2D.objMtxX + cc.B3) & cc.B0;
            const int32_t y0 = (state->rsp->S2D.objMtxY + cc.B3) & cc.B0;
            const int32_t ulx = sprite.objX + cc.A3;
            const int32_t uly = sprite.objY + cc.A3;
            const int32_t lrx = static_cast<int32_t>((((static_cast<uint64_t>(sprite.imageW) - cc.A1) << 8) * (0x80007FFFu / scaleW)) >> 32) + ulx;
            const int32_t lry = static_cast<int32_t>((((static_cast<uint64_t>(sprite.imageH) - cc.A1) << 8) * (0x80007FFFu / scaleH)) >> 32) + uly;

            const int32_t A = state->rsp->S2D.objMtxA;
            const int32_t B = state->rsp->S2D.objMtxB;
            const int32_t C = state->rsp->S2D.objMtxC;
            const int32_t D = state->rsp->S2D.objMtxD;

            // Corners run through the object matrix. Translation and scale keep
            // the quad axis aligned, which the RDP texrect path handles; a
            // rotated one goes out as triangles.
            const int32_t px[4] = {
                x0 + static_cast<int32_t>(((static_cast<int64_t>(ulx) * A + static_cast<int64_t>(uly) * B)) >> 16),
                x0 + static_cast<int32_t>(((static_cast<int64_t>(lrx) * A + static_cast<int64_t>(uly) * B)) >> 16),
                x0 + static_cast<int32_t>(((static_cast<int64_t>(ulx) * A + static_cast<int64_t>(lry) * B)) >> 16),
                x0 + static_cast<int32_t>(((static_cast<int64_t>(lrx) * A + static_cast<int64_t>(lry) * B)) >> 16)
            };
            const int32_t py[4] = {
                y0 + static_cast<int32_t>(((static_cast<int64_t>(ulx) * C + static_cast<int64_t>(uly) * D)) >> 16),
                y0 + static_cast<int32_t>(((static_cast<int64_t>(lrx) * C + static_cast<int64_t>(uly) * D)) >> 16),
                y0 + static_cast<int32_t>(((static_cast<int64_t>(ulx) * C + static_cast<int64_t>(lry) * D)) >> 16),
                y0 + static_cast<int32_t>(((static_cast<int64_t>(lrx) * C + static_cast<int64_t>(lry) * D)) >> 16)
            };

            if ((B != 0) || (C != 0)) {
                drawRotatedSprite(state, sprite, px, py);
                return;
            }

            int32_t left = px[0], right = px[0], top = py[0], bottom = py[0];
            for (int i = 1; i < 4; i++) {
                left = std::min(left, px[i]);
                right = std::max(right, px[i]);
                top = std::min(top, py[i]);
                bottom = std::max(bottom, py[i]);
            }

            // Map the texture the way the S2DEX2 microcode does (read from its
            // RDP output in the gopher64-re replay, docs/rendering.md): texel 0
            // at the quad's first edge and the image's last texel minus one at
            // the far edge, i.e. imageW - 1 texels stretched over the quad. With
            // SHRINKSIZE the last texel is the overlap shared with the next
            // strip; mapping through it (imageW texels) drew a stray row at
            // every strip edge, and stepping at the nominal scale instead let
            // zoomed sprites drift by up to a texel from the hardware image.
            // The step is in s5.10 texels per pixel, the extent in 1/4 pixels.
            const int32_t width = std::max<int32_t>(right - left, 1);
            const int32_t height = std::max<int32_t>(bottom - top, 1);
            int32_t dsdx = (std::max<int32_t>(sprite.imageW - 32, 0) << 7) / width;
            int32_t dtdy = (std::max<int32_t>(sprite.imageH - 32, 0) << 7) / height;
            int32_t s0 = 0;
            int32_t t0 = 0;
            if (sprite.imageFlags & S2DEX_G_BG_FLAG_FLIPS) {
                s0 = sprite.imageW - 32;
                dsdx = -dsdx;
            }
            if (sprite.imageFlags & S2DEX_G_BG_FLAG_FLIPT) {
                t0 = sprite.imageH - 32;
                dtdy = -dtdy;
            }

            // The microcode draws the quad as antialiased RDP triangles, so a
            // row or column the quad only partly covers gets that coverage
            // times the texel alpha (the sprite render modes use
            // CVG_X_ALPHA | ALPHA_CVG_SEL), and is dropped when that falls
            // below one sample. A texrect covers whole pixels, so round each
            // edge to the nearest pixel: a row under half covered is left out
            // instead of drawn at full coverage. Zooming words on the title
            // screen otherwise flashed a line of the transparent palette
            // color (blue) along their bottom edge at the frames where it
            // sat a quarter pixel past a row. Every edge rounds the same way
            // so abutting strips still meet without a gap, and the texture
            // advances to stay evaluated at the new first row and column.
            const int32_t snappedLeft = (left + 2) & ~3;
            const int32_t snappedTop = (top + 2) & ~3;
            s0 += (dsdx * (snappedLeft - left)) >> 7;
            t0 += (dtdy * (snappedTop - top)) >> 7;
            left = snappedLeft;
            top = snappedTop;
            right = (right + 2) & ~3;
            bottom = (bottom + 2) & ~3;

            // The tile stays the full image (setSpriteTile), as in the
            // microcode: with bilerp the hardware does blend toward the next
            // texel row, including on a sprite's one-pixel last strip.
            const int32_t s1 = s0 + ((dsdx * (right - left)) >> 7);
            const int32_t t1 = t0 + ((dtdy * (bottom - top)) >> 7);

            drawSpriteRectStep(state, left, top, right, bottom, s0, t0, dsdx, dtdy);
        }

        void drawObjRectangle(State *state, const uObjSprite &sprite) {
            setSpriteTile(state, sprite);

            const SpriteCorrector cc = spriteCorrector(state->rsp->objRenderMode);
            const int32_t scaleW = std::max<int32_t>(sprite.scaleW, 1);
            const int32_t scaleH = std::max<int32_t>(sprite.scaleH, 1);

            const int32_t xh = (sprite.objX + cc.A2) & cc.B0;
            const int32_t yh = (sprite.objY + cc.A2) & cc.B0;
            const int32_t xl = static_cast<int32_t>((((static_cast<uint64_t>(sprite.imageW) - cc.A1) << 24) * (0x80007FFFu / scaleW)) >> 48) + xh;
            const int32_t yl = static_cast<int32_t>((((static_cast<uint64_t>(sprite.imageH) - cc.A1) << 24) * (0x80007FFFu / scaleH)) >> 48) + yh;

            const int32_t sh = cc.A0 + cc.B2;
            const int32_t sl = sh + sprite.imageW + cc.A0 - cc.A1 - 1;
            const int32_t th = sh - (((yh & 3) * 0x200 * scaleH) >> 16);
            const int32_t tl = th + sprite.imageH + cc.A0 - cc.A1 - 1;

            drawSpriteRect(state, xh, yh, xl, yl, sh, th, sl, tl);
        }

        void objSprite(State *state, DisplayList **dl) {
            uObjSprite sprite;
            readObjSprite(state, (*dl)->w1, sprite);
            drawObjSprite(state, sprite);
        }

        // G_OBJ_RECTANGLE_R carries the object's base scale in addition to the
        // sprite's, so the rect is sized with the folded scale and offset by the
        // matrix origin. Everything else matches the plain rectangle.
        void drawObjRectangleR(State *state, const uObjSprite &sprite) {
            setSpriteTile(state, sprite);

            const SpriteCorrector cc = spriteCorrector(state->rsp->objRenderMode);
            const int32_t baseScaleX = std::max<int32_t>(state->rsp->S2D.objMtxBaseScaleX, 1);
            const int32_t baseScaleY = std::max<int32_t>(state->rsp->S2D.objMtxBaseScaleY, 1);
            const int32_t spriteScaleW = std::max<int32_t>(sprite.scaleW, 1);
            const int32_t spriteScaleH = std::max<int32_t>(sprite.scaleH, 1);
            const int32_t scaleW = std::max<int32_t>(static_cast<int32_t>((static_cast<int64_t>(baseScaleX) * 0x40 * spriteScaleW) >> 16), 1);
            const int32_t scaleH = std::max<int32_t>(static_cast<int32_t>((static_cast<int64_t>(baseScaleY) * 0x40 * spriteScaleH) >> 16), 1);

            const int32_t xhp = static_cast<int32_t>(((static_cast<int64_t>(sprite.objX) << 16) * 0x800 * (0x80007FFFu / baseScaleX)) >> 32)
                + (((state->rsp->S2D.objMtxX + cc.A2) & cc.B0) << 16);
            const int32_t xh = xhp >> 16;
            const int32_t xlp = xhp + static_cast<int32_t>(((static_cast<uint64_t>(sprite.imageW) - static_cast<uint64_t>(static_cast<int64_t>(cc.A1))) << 24) * (0x80007FFFu / scaleW) >> 32);
            const int32_t xl = xlp >> 16;

            const int32_t yhp = static_cast<int32_t>(((static_cast<int64_t>(sprite.objY) << 16) * 0x800 * (0x80007FFFu / baseScaleY)) >> 32)
                + (((state->rsp->S2D.objMtxY + cc.A2) & cc.B0) << 16);
            const int32_t yh = yhp >> 16;
            const int32_t ylp = yhp + static_cast<int32_t>(((static_cast<uint64_t>(sprite.imageH) - static_cast<uint64_t>(static_cast<int64_t>(cc.A1))) << 24) * (0x80007FFFu / scaleH) >> 32);
            const int32_t yl = ylp >> 16;

            const int32_t sh = cc.A0 + cc.B2;
            const int32_t sl = sh + sprite.imageW + cc.A0 - cc.A1 - 1;
            const int32_t th = sh - (((yh & 3) * 0x200 * scaleH) >> 16);
            const int32_t tl = th + sprite.imageH + cc.A0 - cc.A1 - 1;

            int32_t s0 = sh;
            int32_t t0 = th;
            int32_t s1 = sl;
            int32_t t1 = tl;
            if (sprite.imageFlags & S2DEX_G_BG_FLAG_FLIPS) {
                std::swap(s0, s1);
            }
            if (sprite.imageFlags & S2DEX_G_BG_FLAG_FLIPT) {
                std::swap(t0, t1);
            }

            drawSpriteRect(state, xh, yh, xl, yl, s0, t0, s1, t1);
        }

        void objRectangle(State *state, DisplayList **dl) {
            uObjSprite sprite;
            readObjSprite(state, (*dl)->w1, sprite);
            drawObjRectangle(state, sprite);
        }

        void objRectangleR(State *state, DisplayList **dl) {
            uObjSprite sprite;
            readObjSprite(state, (*dl)->w1, sprite);
            drawObjRectangleR(state, sprite);
        }

        void moveMem(State *state, DisplayList **dl) {
            // gSPObjMatrix/gSPObjSubMatrix reuse G_MOVEMEM with their own
            // indices; everything else is a normal F3DEX2 move.
            const uint32_t index = (*dl)->p0(16, 8);
            if (index == 0x17) {
                uObjMtx mtx;
                uint32_t rdramAddress = state->rsp->fromSegmentedMasked((*dl)->w1) & RSP_DMA_MASK;
                memcpy(&mtx, state->fromRDRAM(rdramAddress), sizeof(mtx));
                state->rsp->S2D.objMtxA = mtx.A;
                state->rsp->S2D.objMtxB = mtx.B;
                state->rsp->S2D.objMtxC = mtx.C;
                state->rsp->S2D.objMtxD = mtx.D;
                state->rsp->S2D.objMtxX = mtx.X;
                state->rsp->S2D.objMtxY = mtx.Y;
                state->rsp->S2D.objMtxBaseScaleX = mtx.BaseScaleX;
                state->rsp->S2D.objMtxBaseScaleY = mtx.BaseScaleY;
                return;
            }

            GBI_F3DEX2::moveMem(state, dl);
        }

        void reset(State *state) {
            state->rsp->objRenderMode = 0x0;        }

        void setup(GBI *gbi) {
            gbi->constants = {
                { F3DENUM::G_MTX_MODELVIEW, 0x00 },
                { F3DENUM::G_MTX_PROJECTION, 0x01 },
                { F3DENUM::G_MTX_MUL, 0x00 },
                { F3DENUM::G_MTX_LOAD, 0x02 },
                { F3DENUM::G_MTX_NOPUSH, 0x00 },
                { F3DENUM::G_MTX_PUSH, 0x04 },
                { F3DENUM::G_TEXTURE_ENABLE, 0x00000002 },
                { F3DENUM::G_SHADING_SMOOTH, 0x00000200 },
                { F3DENUM::G_CULL_FRONT, 0x00001000 },
                { F3DENUM::G_CULL_BACK, 0x00002000 },
                { F3DENUM::G_CULL_BOTH, 0x00003000 }
            };

            gbi->map[F3D_G_SPNOOP] = &GBI_EXTENDED::noOpHook;
            gbi->map[S2DEX_G_OBJ_RENDERMODE] = &objRenderMode;
            gbi->map[S2DEX_G_BG_1CYC] = &bg1Cyc;
            gbi->map[S2DEX_G_BG_COPY] = &bgCopy;
            gbi->map[S2DEX_G_OBJ_LOADTXTR] = &objLoadTxtr;
            gbi->map[S2DEX_G_OBJ_LDTX_SPRITE] = &objLoadTxSprite;
            gbi->map[S2DEX_G_OBJ_LDTX_RECT] = &objLoadTxRect;
            gbi->map[S2DEX_G_OBJ_LDTX_RECT_R] = &objLoadTxRectR;
            gbi->map[F3D_G_DL] = &GBI_F3D::runDl;
            gbi->map[F3D_G_ENDDL] = &GBI_F3D::endDl;
            gbi->map[F3D_G_MOVEWORD] = &moveWord;
            gbi->map[F3D_G_SETOTHERMODE_H] = &GBI_F3D::setOtherModeH;
            gbi->map[F3D_G_SETOTHERMODE_L] = &GBI_F3D::setOtherModeL;
            gbi->map[S2DEX_G_RDPHALF_0] = &rdpHalf0;
            gbi->map[F3D_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3D_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3DEX_G_LOAD_UCODE] = &GBI_F3DEX::loadUCode;
            gbi->map[G_SETCIMG] = &GBI_F3D::setColorImage;
            gbi->map[G_SETZIMG] = &GBI_F3D::setDepthImage;
            gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;

            gbi->resetFromTask = &reset;
        }
    }
};