#include "CriLib.h"

namespace CriLib {
	UtfTable::UtfTable(const char *name) {
		row_size = 0;
		row_count = 0;
		addString("<NULL>");
		table_name = addString(name);
	}

	unsigned int UtfTable::getWidth(unsigned int type) {
		if (type <= 1) return 1;
		if (type <= 3) return 2;
		if (type <= 5 || type == 8 || type == 10) return 4;
		return 8;
	}

	unsigned long long UtfTable::getNumber(unsigned int row, const char *name) {
		if (row >= row_count) return 0;

		for (size_t i=0; i<columns.size(); i++) {
			Column &column = columns[i];
			if (column.name != name) continue;

			if ((column.flags & 0xF0) == 0x30) return column.value;
			if ((column.flags & 0xF0) != 0x50) return 0;

			unsigned int width = getWidth(column.flags & 0x0F);

			return readNumber((size_t)row_address + (size_t)row*row_size + column.offset, width);
		}
		return 0;
	}

	string UtfTable::getString(unsigned int row, const char *name) {
		if (row >= row_count) return "";
		for (size_t i=0; i<columns.size(); i++) {
			Column &column = columns[i];
			if (column.name != name) continue;
			if ((column.flags & 0x0F) != 10) return "";

			unsigned long long address = column.value;
			if ((column.flags & 0xF0) == 0x50) {
				address = readNumber((size_t)row_address + (size_t)row*row_size + column.offset, 4);
			}
			else if ((column.flags & 0xF0) != 0x30) return "";
			return readString((size_t)string_address + address);
		}
		return "";
	}

	unsigned int UtfTable::getRowCount() {
		return row_count;
	}

	unsigned long long UtfTable::readNumber(size_t address, unsigned int length) {
		unsigned long long value = 0;
		for (unsigned int i=0; i<length; i++) {
			value = (value << 8) | data[address+i];
		}
		return value;
	}

	string UtfTable::readString(size_t address) {
		if (address >= size) return "";
		const unsigned char *end = (const unsigned char *)memchr(data + address, 0, size - address);
		if (!end) return "";
		return string((const char *)data + address, end - data - address);
	}

	bool UtfTable::read(
		istream &file,
		unsigned long long file_size,
		unsigned long long address,
		const char *signature
	) {
		if (address > file_size || file_size - address < 0x30) return false;
		unsigned char packet[0x10];
		file.seekg((streamoff)address);
		if (!file.read((char *)packet, sizeof(packet))) return false;
		if (memcmp(packet, signature, 4)) return false;

		unsigned long long packet_size = 0;
		for (unsigned int i=0; i<8; i++) {
			packet_size |= (unsigned long long)packet[8+i] << (i*8);
		}
		if (packet_size < 0x20 || packet_size > file_size - address - 0x10) return false;

		size = (size_t)packet_size;
		decoded.resize(size);
		if (!file.read((char *)decoded.data(), (streamsize)size)) return false;
		data = decoded.data();

		if (memcmp(data, "@UTF", 4)) {
			unsigned int key = 0x655F;
			for (size_t i=0; i<size; i++) {
				decoded[i] ^= (unsigned char)key;
				key *= 0x4115;
			}
			data = decoded.data();
		}
		if (memcmp(data, "@UTF", 4)) return false;

		unsigned long long rows = 8 + readNumber(8, 4);
		unsigned long long strings = 8 + readNumber(12, 4);
		if (rows > size || strings > size) return false;

		row_address = (size_t)rows;
		string_address = (size_t)strings;
		row_size = (unsigned int)readNumber(26, 2);
		row_count = (unsigned int)readNumber(28, 4);

		if (row_count && (!row_size || row_count > (size - row_address) / row_size)) return false;

		unsigned int column_count = (unsigned int)readNumber(24, 2);
		size_t column_address = 32;
		unsigned int offset = 0;

		columns.clear();
		columns.reserve(column_count);
		for (unsigned int i=0; i<column_count; i++) {
			if (column_address > size || size - column_address < 5) return false;

			unsigned int flags = data[column_address++];
			unsigned int name_address = (unsigned int)readNumber(column_address, 4);
			column_address += 4;

			unsigned int width = getWidth(flags & 0x0F);

			Column column;
			column.flags = flags;
			column.offset = 0;
			column.value = 0;

			if ((flags & 0xF0) == 0x30) {
				if (width > size - column_address) return false;
				column.value = readNumber(column_address, width);
				column_address += width;
			}
			else if ((flags & 0xF0) == 0x50) {
				column.offset = offset;
				if (width > row_size - offset) return false;
				offset += width;
			}

			if (name_address >= size - string_address) return false;
			column.name = readString((size_t)string_address + name_address);
			columns.push_back(column);
		}

		return true;
	}

	void UtfTable::writeNumber(unsigned char *data, unsigned long long value, unsigned int length) {
		for (unsigned int i=0; i<length; i++) {
			data[length-i-1] = (unsigned char)(value >> (i*8));
		}
	}

	void UtfTable::addColumn(const char *name, unsigned int type) {
		Column column;
		column.name = name;
		column.flags = 0x50 | type;
		column.offset = row_size;
		column.value = 0;
		columns.push_back(column);
		addString(name);
		row_size += getWidth(type);
	}

	unsigned int UtfTable::addString(const string &value) {
		map<string, unsigned int>::iterator it = string_addresses.find(value);
		if (it != string_addresses.end()) return it->second;
		unsigned int address = (unsigned int)strings.size();
		string_addresses[value] = address;
		strings.insert(strings.end(), value.begin(), value.end());
		strings.push_back(0);
		return address;
	}

	void UtfTable::addRow(const vector<unsigned long long> &values) {
		size_t address = row_data.size();
		row_data.resize(address + row_size);
		for (size_t i=0; i<columns.size(); i++) {
			writeNumber(row_data.data() + address + columns[i].offset, values[i], getWidth(columns[i].flags & 0x0F));
		}
		row_count++;
	}

	vector<unsigned char> UtfTable::write(const char *signature) {
		size_t rows = 0x20 + columns.size()*5;
		size_t string_address = rows + row_data.size();
		size_t data_address = (string_address + strings.size() + 7) & ~(size_t)7;
		if (data_address > 0xFFFFFFFF) return vector<unsigned char>();

		vector<unsigned char> output(0x10 + data_address, 0);
		memcpy(output.data(), signature, 4);
		output[4] = 0xFF;
		unsigned long long packet_size = data_address;
		for (unsigned int i=0; i<8; i++) output[8+i] = (unsigned char)(packet_size >> (i*8));
		memcpy(output.data() + 0x10, "@UTF", 4);
		writeNumber(output.data() + 0x14, data_address - 8, 4);
		writeNumber(output.data() + 0x18, rows - 8, 4);
		writeNumber(output.data() + 0x1C, string_address - 8, 4);
		writeNumber(output.data() + 0x20, data_address - 8, 4);
		writeNumber(output.data() + 0x24, table_name, 4);
		writeNumber(output.data() + 0x28, columns.size(), 2);
		writeNumber(output.data() + 0x2A, row_size, 2);
		writeNumber(output.data() + 0x2C, row_count, 4);
		for (size_t i=0; i<columns.size(); i++) {
			output[0x30+i*5] = (unsigned char)columns[i].flags;
			writeNumber(output.data() + 0x31+i*5, string_addresses[columns[i].name], 4);
		}
		copy(row_data.begin(), row_data.end(), output.begin() + 0x10 + rows);
		copy(strings.begin(), strings.end(), output.begin() + 0x10 + string_address);
		return output;
	}
};
