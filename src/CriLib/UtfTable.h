#pragma once

namespace CriLib {
	class UtfTable {
		struct Column {
			string name;
			unsigned int flags;
			unsigned int offset;
			unsigned long long value;
		};

		const unsigned char *data;
		size_t size;
		vector<unsigned char> decoded;
		vector<Column> columns;
		size_t row_address;
		size_t string_address;
		unsigned int row_size;
		unsigned int row_count;
		unsigned int table_name;
		vector<unsigned char> row_data;
		vector<unsigned char> strings;
		map<string, unsigned int> string_addresses;

		unsigned int getWidth(unsigned int type);
		unsigned long long readNumber(size_t address, unsigned int length);
		string readString(size_t address);

		void writeNumber(
			unsigned char *data,
			unsigned long long value,
			unsigned int length
		);

	public:
		UtfTable(const char *name="");

		unsigned long long getNumber(unsigned int row, const char *name);
		string getString(unsigned int row, const char *name);
		unsigned int getRowCount();

		bool read(
			istream &file,
			unsigned long long file_size,
			unsigned long long address,
			const char *signature
		);

		void addColumn(const char *name, unsigned int type);
		unsigned int addString(const string &value);
		void addRow(const vector<unsigned long long> &values);
		vector<unsigned char> write(const char *signature);
	};
};
