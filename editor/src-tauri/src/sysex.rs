/// Encode binary bytes into MIDI SysEx-safe 7-bit format.
/// Every 7 input bytes → 8 output bytes (1 MSB byte + 7 data bytes).
pub fn encode_7bit(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(((input.len() + 6) / 7) * 8);
    for chunk in input.chunks(7) {
        let msb: u8 = chunk
            .iter()
            .enumerate()
            .fold(0u8, |acc, (i, &b)| acc | (((b >> 7) & 1) << i));
        out.push(msb);
        for &b in chunk {
            out.push(b & 0x7F);
        }
    }
    out
}

/// Decode 7-bit-encoded SysEx bytes back to binary.
pub fn decode_7bit(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < input.len() {
        let remaining = input.len() - i;
        if remaining < 2 { break; }
        let chunk_len = (remaining - 1).min(7);
        let msb = input[i]; i += 1;
        for j in 0..chunk_len {
            out.push(input[i] | (((msb >> j) & 1) << 7));
            i += 1;
        }
    }
    out
}

pub fn build_get_preset(bank: u8, slot: u8) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x01, bank, slot, 0xF7]
}

pub fn build_put_preset(bank: u8, slot: u8, name: &str, raw_data: &[u8; 92]) -> Vec<u8> {
    let mut name_bytes = [0u8; 12];
    for (i, b) in name.bytes().take(11).enumerate() { name_bytes[i] = b; }
    let encoded = encode_7bit(raw_data);
    let mut frame = vec![0xF0, 0x7D, 0x02, bank, slot];
    frame.extend_from_slice(&name_bytes);
    frame.extend_from_slice(&encoded);
    frame.push(0xF7);
    frame
}

pub fn build_set_active(bank: u8, slot: u8) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x04, bank, slot, 0xF7]
}

pub fn build_get_all() -> Vec<u8> {
    vec![0xF0, 0x7D, 0x05, 0xF7]
}

pub fn build_set_mode(stage: u8, mode_index: u8) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x07, stage, mode_index, 0xF7]
}

pub fn build_set_fx_enabled(stage: u8, enabled: bool) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x08, stage, if enabled { 0x01 } else { 0x00 }, 0xF7]
}

pub fn build_cc(channel: u8, cc: u8, value: u8) -> Vec<u8> {
    vec![0xB0 | (channel & 0x0F), cc, value & 0x7F]
}

/// Parse a PRESET_DATA response (cmd 0x81).
/// Returns (bank, slot, name, raw_92_bytes) or None if malformed.
pub fn parse_preset_data(frame: &[u8]) -> Option<(u8, u8, String, Vec<u8>)> {
    // frame: F0 7D 81 bank slot name[12] encoded[106] F7
    if frame.len() < 5 || frame[0] != 0xF0 || frame[1] != 0x7D || frame[2] != 0x81 { return None; }
    let bank = frame[3];
    let slot = frame[4];
    if frame.len() < 5 + 12 { return None; }
    let name = String::from_utf8_lossy(&frame[5..17]).trim_end_matches('\0').to_string();
    let raw = decode_7bit(&frame[17..frame.len().saturating_sub(1)]);
    Some((bank, slot, name, raw))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_92_bytes() {
        let input: Vec<u8> = (0u8..=91).collect();
        let encoded = encode_7bit(&input);
        let decoded = decode_7bit(&encoded);
        assert_eq!(decoded, input);
        // 92 bytes: 13 full groups (104 bytes) + 1 remainder (2 bytes) = 106
        let n = input.len();
        let expected = (n / 7) * 8 + if n % 7 > 0 { n % 7 + 1 } else { 0 };
        assert_eq!(encoded.len(), expected);
    }

    #[test]
    fn all_high_bits() {
        let input = vec![0xFF; 92];
        let encoded = encode_7bit(&input);
        assert!(encoded.iter().all(|&b| b < 0x80), "all output bytes must be < 0x80");
        let decoded = decode_7bit(&encoded);
        assert_eq!(decoded, input);
    }
}
