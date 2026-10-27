#include "CriLib.h"

namespace CriLib {
	const unsigned int length_bits[] = {2, 3, 5, 8};

	unsigned int CriLayla::readBits(unsigned int count) {
		unsigned int value = 0;

		while (count) {
			if (!remaining) {
				if (address == buffer + 0x10) return 0xFFFFFFFF;
				bits = *--address;
				remaining = 8;
			}

			unsigned int length = count < remaining ? count : remaining;
			remaining -= length;
			value = (value << length) | ((bits >> remaining) & ((1u << length) - 1));
			count -= length;
		}

		return value;
	}

	bool CriLayla::decompress(
		const unsigned char *buffer,
		size_t size,
		unsigned char *data,
		size_t extract_size
	) {
		if (size < 0x110 || memcmp(buffer, "CRILAYLA", 8)) return false;

		unsigned int length = buffer[8] | (buffer[9] << 8) |
			(buffer[10] << 16) | ((unsigned int)buffer[11] << 24);
		unsigned int header = buffer[12] | (buffer[13] << 8) |
			(buffer[14] << 16) | ((unsigned int)buffer[15] << 24);

		if ((size_t)length + 0x100 != extract_size) return false;
		if ((size_t)header > size - 0x110) return false;

		memcpy(data, buffer + 0x10 + header, 0x100);

		CriLayla reader;
		reader.buffer = buffer;
		reader.address = buffer + 0x10 + header;
		reader.bits = 0;
		reader.remaining = 0;

		size_t position = extract_size;

		while (position > 0x100) {
			unsigned int value = reader.readBits(1);
			if (value == 0xFFFFFFFF) return false;

			if (!value) {
				value = reader.readBits(8);
				if (value == 0xFFFFFFFF) return false;
				data[--position] = (unsigned char)value;
			}
			else {
				value = reader.readBits(13);
				if (value == 0xFFFFFFFF) return false;

				size_t distance = value + 3;
				size_t run = 3;

				for (unsigned int i=0; ; ) {
					unsigned int count = length_bits[i];
					value = reader.readBits(count);
					if (value == 0xFFFFFFFF) return false;
					run += value;
					if (value != ((1u << count) - 1)) break;
					if (i < 3) i++;
				}

				if (run > position - 0x100 || distance > extract_size - position) return false;

				if (run <= distance) {
					position -= run;
					memcpy(data + position, data + position + distance, run);
				}
				else {
					while (run--) {
						--position;
						data[position] = data[position + distance];
					}
				}
			}
		}

		return true;
	}

	void CriLayla::writeBits(vector<unsigned char> &data, unsigned int value, unsigned int count) {
		bits = (bits << count) | value;
		unsigned int length = 8 - remaining + count;
		while (length >= 8) {
			length -= 8;
			data.push_back((unsigned char)(bits >> length));
		}
		remaining = 8 - length;
	}

	bool CriLayla::compress(
		const unsigned char *data,
		size_t size,
		vector<unsigned char> &buffer
	) {
		buffer.clear();
		if (size < 0x100 || size - 0x100 > 0xFFFFFFFF) return false;

		buffer.reserve(size + (size - 0x100 + 7)/8 + 0x10);
		buffer.resize(0x10);
		vector<size_t> table(0x10000, 0);
		vector<size_t> links(0x4000, 0);
		CriLayla writer;
		writer.bits = 0;
		writer.remaining = 8;

		size_t position = size;
		while (position > 0x100) {
			size_t at = position - 1;
			size_t run = 1;
			size_t distance = 0;
			if (position >= 0x103) {
				unsigned int index = ((data[at]*251u + data[at-1])*251u + data[at-2]) & 0xFFFF;
				size_t offset = table[index];
				while (offset && offset - 1 - at <= 0x2002) {
					size_t address = offset - 1;
					if (address - at >= 3 && data[at-run] == data[address-run]) {
						size_t length = 0;
						while (length + 8 <= position - 0x100) {
							unsigned long long a, b;
							memcpy(&a, data + position - length - 8, 8);
							memcpy(&b, data + address + 1 - length - 8, 8);
							if (a != b) break;
							length += 8;
						}
						while (length < position - 0x100 && data[at-length] == data[address-length]) length++;
						if (length >= 3 && length > run) {
							run = length;
							distance = address - at;
							if (run == position - 0x100) break;
						}
					}
					offset = links[address & 0x3FFF];
				}
			}

			if (run >= 3) {
				writer.writeBits(buffer, 0x2000 | ((unsigned int)distance - 3), 14);
				size_t length = run - 3;
				for (unsigned int i=0; ; ) {
					unsigned int count = length_bits[i];
					unsigned int limit = (1u << count) - 1;
					unsigned int value = length < limit ? (unsigned int)length : limit;
					writer.writeBits(buffer, value, count);
					length -= value;
					if (value != limit) break;
					if (i < 3) i++;
				}
			}
			else {
				writer.writeBits(buffer, data[at], 9);
			}

			for (size_t i=0; i<run && at-i>=0x102; i++) {
				size_t address = at - i;
				unsigned int index = ((data[address]*251u + data[address-1])*251u + data[address-2]) & 0xFFFF;
				links[address & 0x3FFF] = table[index];
				table[index] = address + 1;
			}
			position -= run;
		}

		if (writer.remaining != 8) buffer.push_back((unsigned char)(writer.bits << writer.remaining));
		size_t header = buffer.size() - 0x10;
		if (header > 0xFFFFFFFF) return false;
		for (size_t i=0; i<header/2; i++) {
			unsigned char value = buffer[0x10+i];
			buffer[0x10+i] = buffer[0x10+header-i-1];
			buffer[0x10+header-i-1] = value;
		}
		buffer.insert(buffer.end(), data, data + 0x100);
		memcpy(buffer.data(), "CRILAYLA", 8);
		for (unsigned int i=0; i<4; i++) {
			buffer[8+i] = (unsigned char)((size - 0x100) >> (i*8));
			buffer[12+i] = (unsigned char)(header >> (i*8));
		}
		return true;
	}
};
