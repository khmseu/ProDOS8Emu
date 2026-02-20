#pragma once

#include <cstdint>

namespace prodos8emu {

  /**
   * ProDOS 8 MLI Error Codes
   *
   * As documented in ProDOS 8 Technical Reference Manual, Section 4.8
   * "MLI Error Codes"
   *
   * These error codes are returned by MLI calls to indicate success or failure.
   */

  // No error
  constexpr uint8_t ERR_NO_ERROR = 0x00;

  // Invalid MLI call number
  constexpr uint8_t ERR_BAD_CALL_NUMBER = 0x01;

  // Parameter count doesn't match call requirements
  constexpr uint8_t ERR_BAD_CALL_PARAM_COUNT = 0x04;

  // Interrupt table full
  constexpr uint8_t ERR_INTERRUPT_TABLE_FULL = 0x25;

  // I/O error occurred
  constexpr uint8_t ERR_IO_ERROR = 0x27;

  // Device not connected
  constexpr uint8_t ERR_NO_DEVICE = 0x28;

  // Write protected
  constexpr uint8_t ERR_WRITE_PROTECTED = 0x2B;

  // Disk switched
  constexpr uint8_t ERR_DISK_SWITCHED = 0x2E;

  // Invalid pathname syntax
  constexpr uint8_t ERR_INVALID_PATH_SYNTAX = 0x40;

  // Maximum number of files open (canonical name per TechRef)
  constexpr uint8_t ERR_TOO_MANY_FILES_OPEN = 0x42;
  // Alias for backward compatibility
  constexpr uint8_t ERR_FCB_ERROR = 0x42;

  // Invalid reference number
  constexpr uint8_t ERR_BAD_REF_NUM = 0x43;

  // Path not found (also known as directory not found)
  constexpr uint8_t ERR_PATH_NOT_FOUND = 0x44;
  // Alias for clarity in some contexts
  constexpr uint8_t ERR_DIR_NOT_FOUND = 0x44;

  // Volume not found
  constexpr uint8_t ERR_VOL_NOT_FOUND = 0x45;

  // File not found
  constexpr uint8_t ERR_FILE_NOT_FOUND = 0x46;

  // Duplicate filename in directory
  constexpr uint8_t ERR_DUPLICATE_FILENAME = 0x47;

  // Volume full (canonical name per TechRef: disk overrun)
  constexpr uint8_t ERR_VOLUME_FULL = 0x48;
  // Alias for backward compatibility
  constexpr uint8_t ERR_OVERRUN_ERROR = 0x48;

  // Volume directory full
  constexpr uint8_t ERR_VOL_DIR_FULL = 0x49;

  // Incompatible file format (wrong version)
  constexpr uint8_t ERR_INCOMPATIBLE_VERSION = 0x4A;

  // Unsupported storage type
  constexpr uint8_t ERR_UNSUPPORTED_STOR_TYPE = 0x4B;

  // End of file encountered
  constexpr uint8_t ERR_EOF_ENCOUNTERED = 0x4C;

  // Position out of range
  constexpr uint8_t ERR_POSITION_OUT_OF_RANGE = 0x4D;

  // Access not allowed by access bits
  constexpr uint8_t ERR_ACCESS_ERROR = 0x4E;

  // File is open
  constexpr uint8_t ERR_FILE_OPEN = 0x50;

  // Directory structure damaged
  constexpr uint8_t ERR_DIR_COUNT_ERROR = 0x51;

  // Not a ProDOS volume
  constexpr uint8_t ERR_NOT_PRODOS_VOL = 0x52;

  // Invalid parameter value
  constexpr uint8_t ERR_INVALID_PARAMETER = 0x53;

  // Volume control block table full
  constexpr uint8_t ERR_VCB_TABLE_FULL = 0x55;

  // Bad buffer address
  constexpr uint8_t ERR_BAD_BUFFER_ADDR = 0x56;

  // Duplicate volume name
  constexpr uint8_t ERR_DUPLICATE_VOLUME = 0x57;

  // File structure damaged (canonical name per TechRef)
  constexpr uint8_t ERR_FILE_STRUCTURE_DAMAGED = 0x5A;
  // Alias for backward compatibility
  constexpr uint8_t ERR_BAD_FILE_FORMAT = 0x5A;

}  // namespace prodos8emu
