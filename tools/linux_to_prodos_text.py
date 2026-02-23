#!/usr/bin/env python3
"""Convert Linux text files to ProDOS TEXT format.

Phase 1: Core byte conversion functions for line endings and ASCII.
"""


def normalize_line_endings(data: bytes) -> bytes:
    """Normalize line endings to ProDOS CR (\\r).
    
    Converts:
    - CRLF (\\r\\n) -> CR (\\r)
    - LF (\\n) -> CR (\\r)
    - Preserves standalone CR (\\r)
    
    Args:
        data: Input bytes to normalize
        
    Returns:
        Bytes with normalized line endings
    """
    # Replace CRLF with CR first to avoid double conversion
    data = data.replace(b'\r\n', b'\r')
    # Then replace remaining LF with CR
    data = data.replace(b'\n', b'\r')
    return data


def convert_to_ascii(data: bytes, *, strict: bool) -> bytes:
    """Convert data to ASCII, handling high-bit characters.
    
    Args:
        data: Input bytes
        strict: If True, raise ValueError on non-ASCII (>= 0x80).
                If False, replace non-ASCII bytes with ord('?').
                
    Returns:
        ASCII-only bytes
        
    Raises:
        ValueError: If strict=True and non-ASCII bytes found
    """
    # Check for non-ASCII bytes
    non_ascii_found = any(byte >= 0x80 for byte in data)
    
    if non_ascii_found and strict:
        raise ValueError("Input contains non-ASCII bytes (>= 0x80)")
    
    if non_ascii_found:
        # Replace all bytes >= 0x80 with '?'
        result = bytearray()
        for byte in data:
            if byte >= 0x80:
                result.append(ord('?'))
            else:
                result.append(byte)
        return bytes(result)
    
    return data
