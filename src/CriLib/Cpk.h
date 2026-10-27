#pragma once

namespace CriLib {
	class Cpk;

	class CpkFile {
	protected:
		Cpk *archive;
		string name;
		unsigned long long address;
		unsigned long long size;
		unsigned long long extract_size;
		vector<unsigned char> data;

	public:
		CpkFile(
			Cpk *archive,
			string filename,
			unsigned long long address,
			unsigned long long size,
			unsigned long long extract_size
		);

		const string& getName();
		unsigned char *getData();
		unsigned long long getSize();
		unsigned long long getAddress();

		bool read();

		bool save(string filename);
	};

	class Cpk {
		friend class CpkFile;

	protected:
		string filename;
		unsigned long long size;
		vector<CpkFile *> files;
		unsigned int version;
		unsigned int revision;
		bool valid;

		static int compareNames(const string &a, const string &b);
		static bool readFolder(
			const filesystem::path &folder,
			const filesystem::path &filename,
			vector<string> &names
		);

		static bool writeData(
			ostream &file,
			const unsigned char *data,
			size_t size
		);
		static bool writeHeader(
			ostream &file,
			unsigned long long address,
			size_t toc_size,
			unsigned long long packed_size,
			unsigned long long data_size,
			size_t files,
			unsigned int version,
			unsigned int revision
		);

	public:
		Cpk(string filename);
		~Cpk();

		CpkFile *getFile(string filename);
		CpkFile *getFileByIndex(size_t index);
		size_t getFileCount();

		bool read();
		bool extract(string folder, unsigned int threads=0);

		static bool pack(
			string folder,
			string filename,
			bool m_isCriLaylaCompressed=false
		);
	};
};
