//===-- Cpu0ASMBackend.cpp - Cpu0 Asm Backend  ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Cpu0AsmBackend and Cpu0ELFObjectWriter classes.
//
//===----------------------------------------------------------------------===//
//

#include "MCTargetDesc/Cpu0FixupKinds.h"
#include "MCTargetDesc/Cpu0AsmBackend.h"
#if CH >= CH5_1

#include "MCTargetDesc/Cpu0MCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//@adjustFixupValue {
// Prepare value for the target space for it
static unsigned adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext *Ctx = nullptr) {

  unsigned Kind = Fixup.getKind();

  // Add/subtract and shift
  switch (Kind) {
  default:
    return 0;
  case FK_GPRel_4:
  case FK_Data_4:
#if CH >= CH9_1
  case Cpu0::fixup_Cpu0_CALL16:
#endif
  case Cpu0::fixup_Cpu0_LO16:
#if CH >= CH6_1
  case Cpu0::fixup_Cpu0_GOT_LO16:
#endif
    break;
#if CH >= CH8_1 //1
  case Cpu0::fixup_Cpu0_PC16:
  case Cpu0::fixup_Cpu0_PC24:
    // So far we are only using this type for branches and jump.
    // For branches we start 1 instruction after the branch
    // so the displacement will be one instruction size less.
    Value -= 4;
    break;
#endif
  case Cpu0::fixup_Cpu0_HI16:
  case Cpu0::fixup_Cpu0_GOT_Local:
#if CH >= CH6_1
  case Cpu0::fixup_Cpu0_GOT_HI16:
#endif
    // Get the higher 16-bits. Also add 1 if bit 15 is 1.
    Value = ((Value + 0x8000) >> 16) & 0xffff;
    break;
  }

  return Value;
}
//@adjustFixupValue }

MCObjectWriter *Cpu0AsmBackend::createObjectWriter(raw_ostream &OS) const {
  return createCpu0ELFObjectWriter(OS,
    MCELFObjectTargetWriter::getOSABI(OSType), IsLittle);
}

/// ApplyFixup - Apply the \arg Value for given \arg Fixup into the provided
/// data fragment, at the offset specified by the fixup and following the
/// fixup kind as appropriate.
void Cpu0AsmBackend::applyFixup(const MCFixup &Fixup, char *Data,
                                unsigned DataSize, uint64_t Value,
                                bool IsPCRel) const {
  MCFixupKind Kind = Fixup.getKind();
  Value = adjustFixupValue(Fixup, Value);

  if (!Value)
    return; // Doesn't change encoding.

  // Where do we start in the object
  unsigned Offset = Fixup.getOffset();
  // Number of bytes we need to fixup
  unsigned NumBytes = (getFixupKindInfo(Kind).TargetSize + 7) / 8;
  // Used to point to big endian bytes
  unsigned FullSize;

  switch ((unsigned)Kind) {
  default:
    FullSize = 4;
    break;
  }

  // Grab current value, if any, from bits.
  uint64_t CurVal = 0;

  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = IsLittle ? i : (FullSize - 1 - i);
    CurVal |= (uint64_t)((uint8_t)Data[Offset + Idx]) << (i*8);
  }

  uint64_t Mask = ((uint64_t)(-1) >> (64 - getFixupKindInfo(Kind).TargetSize));
  CurVal |= Value & Mask;

  // Write out the fixed up bytes back to the code/data bits.
  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = IsLittle ? i : (FullSize - 1 - i);
    Data[Offset + Idx] = (uint8_t)((CurVal >> (i*8)) & 0xff);
  }
}

//@getFixupKindInfo {
const MCFixupKindInfo &Cpu0AsmBackend::
getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[Cpu0::NumTargetFixupKinds] = {
    // This table *must* be in same the order of fixup_* kinds in
    // Cpu0FixupKinds.h.
    //
    // name                        offset  bits  flags
    { "fixup_Cpu0_32",             0,     32,   0 },
    { "fixup_Cpu0_HI16",           0,     16,   0 },
    { "fixup_Cpu0_LO16",           0,     16,   0 },
    { "fixup_Cpu0_GPREL16",        0,     16,   0 },
    { "fixup_Cpu0_GOT_Global",     0,     16,   0 },
    { "fixup_Cpu0_GOT_Local",      0,     16,   0 },
#if CH >= CH8_1 //2
    { "fixup_Cpu0_PC16",           0,     16,  MCFixupKindInfo::FKF_IsPCRel },
    { "fixup_Cpu0_PC24",           0,     24,  MCFixupKindInfo::FKF_IsPCRel },
#endif
#if CH >= CH9_1
    { "fixup_Cpu0_CALL16",         0,     16,   0 },
#endif
#if CH >= CH12_1
    { "fixup_Cpu0_TLSGD",          0,     16,   0 },
    { "fixup_Cpu0_GOTTP",          0,     16,   0 },
    { "fixup_Cpu0_TP_HI",          0,     16,   0 },
    { "fixup_Cpu0_TP_LO",          0,     16,   0 },
    { "fixup_Cpu0_TLSLDM",         0,     16,   0 },
    { "fixup_Cpu0_DTP_HI",         0,     16,   0 },
    { "fixup_Cpu0_DTP_LO",         0,     16,   0 },
#endif
    { "fixup_Cpu0_GOT_HI16",       0,     16,   0 },
    { "fixup_Cpu0_GOT_LO16",       0,     16,   0 }
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}
//@getFixupKindInfo }

/// WriteNopData - Write an (optimal) nop sequence of Count bytes
/// to the given output. If the target cannot generate such a sequence,
/// it should return an error.
///
/// \return - True on success.
bool Cpu0AsmBackend::writeNopData(uint64_t Count, MCObjectWriter *OW) const {
  return true;
}

// MCAsmBackend
MCAsmBackend *llvm::createCpu0AsmBackendEL32(const Target &T,
                                             const MCRegisterInfo &MRI,
                                             StringRef TT,
                                             StringRef CPU) {
  return new Cpu0AsmBackend(T, Triple(TT).getOS(),
                            /*IsLittle*/true);
}

MCAsmBackend *llvm::createCpu0AsmBackendEB32(const Target &T,
                                             const MCRegisterInfo &MRI,
                                             StringRef TT,
                                             StringRef CPU) {
  return new Cpu0AsmBackend(T, Triple(TT).getOS(),
                            /*IsLittle*/false);
}

#endif // #if CH >= CH5_1
