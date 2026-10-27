#include "CriLib.h"

namespace CriLib {
	CpkFile::CpkFile(
		Cpk *archive,
		string filename,
		unsigned long long address,
		unsigned long long size,
		unsigned long long extract_size
	) {
		this->archive = archive;
		name = filename;
		this->address = address;
		this->size = size;
		this->extract_size = extract_size;
	}

	const string& CpkFile::getName() {
		return name;
	}

	unsigned char *CpkFile::getData() {
		if (!read() || data.empty()) return NULL;
		return data.data();
	}

	unsigned long long CpkFile::getSize() {
		return extract_size;
	}

	unsigned long long CpkFile::getAddress() {
		return address;
	}

	Cpk::Cpk(string filename) {
		this->filename = filename;
		size = 0;
		version = 7;
		revision = 14;
		valid = false;

		ifstream file(filesystem::u8path(filename), ios::binary | ios::ate);
		if (!file || file.tellg() <= 0) return;
		size = (unsigned long long)file.tellg();
		valid = read();
	}

	Cpk::~Cpk() {
		for (size_t i=0; i<files.size(); i++) delete files[i];
	}

	CpkFile *Cpk::getFile(string filename) {
		for (size_t i=0; i<files.size(); i++) {
			if (files[i]->getName() == filename) return files[i];
		}
		return NULL;
	}

	CpkFile *Cpk::getFileByIndex(size_t index) {
		return index < files.size() ? files[index] : NULL;
	}

	size_t Cpk::getFileCount() {
		return files.size();
	}

	int Cpk::compareNames(const string &a, const string &b) {
		size_t i = 0;
		while (i < a.size() && i < b.size()) {
			int order = tolower((unsigned char)a[i]) - tolower((unsigned char)b[i]);
			if (order) return order;
			i++;
		}
		return (i < a.size()) - (i < b.size());
	}

	bool CpkFile::read() {
		if (data.size() == extract_size) return true;

		ifstream file(filesystem::u8path(archive->filename), ios::binary);
		if (!file.seekg((streamoff)address)) return false;
		data.resize((size_t)extract_size);
		bool success;
		if (size == extract_size) {
			success = (bool)file.read((char *)data.data(), (streamsize)size);
		}
		else {
			vector<unsigned char> compressed((size_t)size);
			success = (bool)file.read((char *)compressed.data(), (streamsize)size);
			if (success) success = CriLayla::decompress(compressed.data(), compressed.size(), data.data(), data.size());
		}
		if (success) return true;
		data.clear();
		return false;
	}

	bool Cpk::read() {
		for (size_t i=0; i<files.size(); i++) delete files[i];
		files.clear();

		ifstream file(filesystem::u8path(filename), ios::binary);
		if (!file) return false;
		UtfTable header;
		UtfTable toc;
		if (!header.read(file, size, 0, "CPK ")) return false;
		version = (unsigned int)header.getNumber(0, "Version");
		revision = (unsigned int)header.getNumber(0, "Revision");

		unsigned long long toc_address = header.getNumber(0, "TocOffset");
		unsigned long long content_address = header.getNumber(0, "ContentOffset");
		if (!toc_address || !content_address) return false;
		if (!toc.read(file, size, toc_address, "TOC ")) return false;

		unsigned long long base = toc_address < content_address ? toc_address : content_address;

		for (unsigned int i=0; i<toc.getRowCount(); i++) {
			string name = toc.getString(i, "FileName");
			string folder = toc.getString(i, "DirName");
			if (!folder.empty() && folder != "<NULL>") name = folder + "/" + name;

			for (size_t j=0; j<name.size(); j++) {
				if (name[j] == '\\') name[j] = '/';
			}
			if (name.empty() || name[0] == '/' || name.find(':') != string::npos) return false;
			for (size_t start=0; start<name.size(); ) {
				size_t end = name.find('/', start);
				if (name.substr(start, end - start) == "..") return false;
				if (end == string::npos) break;
				start = end + 1;
			}

			unsigned long long file_size = toc.getNumber(i, "FileSize");
			unsigned long long extract_size = toc.getNumber(i, "ExtractSize");
			if (!extract_size) extract_size = file_size;
			unsigned long long offset = toc.getNumber(i, "FileOffset");

			if (offset > size - base) return false;
			unsigned long long address = base + offset;
			if (file_size > size - address) return false;

			files.push_back(new CpkFile(this, name, address, file_size, extract_size));
		}
		return true;
	}

	bool Cpk::extract(string folder, unsigned int threads) {
		if (!valid) return false;
		for (size_t i=0; i<files.size(); i++) {
			if (!compareNames(files[i]->getName(), "CriLib.xml")) return false;
		}

		vector<CpkFile *> names = files;
		sort(names.begin(), names.end(), [](CpkFile *a, CpkFile *b) {
			return compareNames(a->getName(), b->getName()) < 0;
		});
		for (size_t i=1; i<names.size(); i++) {
			if (!compareNames(names[i-1]->getName(), names[i]->getName())) return false;
		}

		error_code error;
		filesystem::path path = filesystem::u8path(folder);
		filesystem::create_directories(path, error);
		if (error) return false;

		for (size_t i=0; i<files.size(); i++) {
			filesystem::create_directories((path / filesystem::u8path(files[i]->getName())).parent_path(), error);
			if (error) return false;
		}

		sort(names.begin(), names.end(), [](CpkFile *a, CpkFile *b) {
			return a->getAddress() < b->getAddress();
		});
		if (!threads) {
			threads = thread::hardware_concurrency();
			if (threads > 8) threads = 8;
		}
		if (threads > names.size()) threads = (unsigned int)names.size();
		if (!threads) threads = 1;
		vector<unsigned char> success(threads, 1);
		auto extract = [&](unsigned int index) {
			for (size_t i=index; i<names.size(); i+=threads) {
				if (!names[i]->save((path / filesystem::u8path(names[i]->getName())).u8string())) success[index] = 0;
			}
		};

		vector<thread> workers;
		workers.reserve(threads);
		try {
			for (unsigned int i=1; i<threads; i++) workers.push_back(thread(extract, i));
		}
		catch (const system_error &) {
			for (unsigned int i=(unsigned int)workers.size()+1; i<threads; i++) {
				extract(i);
			}
		}
		extract(0);

		for (size_t i=0; i<workers.size(); i++) workers[i].join();
		if (find(success.begin(), success.end(), 0) != success.end()) return false;

		filesystem::path xml_path = path / "CriLib.xml";
		if (filesystem::equivalent(xml_path, filesystem::u8path(filename), error)) return false;
		return CriLibXml::write(xml_path, version, revision);
	}

	bool Cpk::readFolder(
		const filesystem::path &folder,
		const filesystem::path &filename,
		vector<string> &names
	) {
		error_code error;
		filesystem::recursive_directory_iterator it(folder, error);
		filesystem::recursive_directory_iterator end;
		while (!error && it != end) {
			filesystem::file_status status = it->symlink_status(error);
			if (error || filesystem::is_symlink(status)) return false;
			if (filesystem::is_regular_file(status)) {
				if (it->path() == filename || filesystem::equivalent(it->path(), filename, error)) return false;
				error.clear();
				string name = it->path().lexically_relative(folder).generic_u8string();
				if (compareNames(name, "CriLib.xml")) names.push_back(name);
			}
			else if (!filesystem::is_directory(status)) return false;
			it.increment(error);
		}
		return !error && names.size() <= 0xFFFFFFFF;
	}

	bool Cpk::writeData(ostream &file, const unsigned char *data, size_t size) {
		while (size) {
			size_t length = size > 0x800000 ? 0x800000 : size;
			if (!file.write((const char *)data, (streamsize)length)) return false;
			data += length;
			size -= length;
		}
		return true;
	}

	bool CpkFile::save(string filename) {
		error_code error;
		filesystem::path path = filesystem::u8path(filename);
		if (filesystem::equivalent(path, filesystem::u8path(archive->filename), error)) return false;
		ifstream input(filesystem::u8path(archive->filename), ios::binary);
		if (!input.seekg((streamoff)address)) return false;
		ofstream file(path, ios::binary | ios::trunc);
		if (!file) return false;

		bool success = true;
		if (size == extract_size) {
			vector<unsigned char> buffer((size_t)min(size, 0x800000ULL));
			unsigned long long left = size;
			while (success && left) {
				size_t length = (size_t)min(left, (unsigned long long)buffer.size());
				success = (bool)input.read((char *)buffer.data(), (streamsize)length);
				if (success) success = Cpk::writeData(file, buffer.data(), length);
				left -= length;
			}
		}
		else {
			vector<unsigned char> compressed((size_t)size);
			vector<unsigned char> output((size_t)extract_size);
			success = (bool)input.read((char *)compressed.data(), (streamsize)size);
			if (success) success = CriLayla::decompress(compressed.data(), compressed.size(), output.data(), output.size());
			if (success) success = Cpk::writeData(file, output.data(), output.size());
		}

		file.close();
		if (!file) success = false;
		if (!success) filesystem::remove(path, error);
		return success;
	}

	bool Cpk::writeHeader(
		ostream &file,
		unsigned long long address,
		size_t toc_size,
		unsigned long long packed_size,
		unsigned long long data_size,
		size_t files,
		unsigned int version,
		unsigned int revision
	) {
		UtfTable header("CpkHeader");
		struct HeaderColumn {
			const char *name;
			unsigned int type;
			unsigned long long value;
		};
		HeaderColumn columns[] = {
			{"UpdateDateTime", 6, 0},
			{"FileSize", 6, address + toc_size},
			{"ContentOffset", 6, 0x800},
			{"ContentSize", 6, address - 0x800},
			{"TocOffset", 6, address},
			{"TocSize", 6, toc_size},
			{"EtocOffset", 6, 0},
			{"EtocSize", 6, 0},
			{"ItocOffset", 6, 0},
			{"ItocSize", 6, 0},
			{"GtocOffset", 6, 0},
			{"GtocSize", 6, 0},
			{"EnabledPackedSize", 6, packed_size},
			{"EnabledDataSize", 6, data_size},
			{"TotalDataSize", 6, data_size},
			{"Tocs", 4, 1},
			{"Files", 4, files},
			{"Groups", 4, 0},
			{"Attrs", 4, 0},
			{"TotalFiles", 4, files},
			{"Directories", 4, 0},
			{"Updates", 4, 0},
			{"Version", 2, version},
			{"Revision", 2, revision},
			{"Align", 2, 0x800},
			{"Sorted", 2, 1},
			{"EnableFileName", 2, 1},
			{"CpkMode", 4, 1},
			{"Tvers", 10, header.addString("CriLib")},
			{"Comment", 10, 0},
			{"Codec", 4, 0},
			{"DpkItoc", 4, 0},
			{"EnableTocCrc", 2, 0},
			{"EnableFileCrc", 2, 0},
			{"CrcMode", 4, 0}
		};
		vector<unsigned long long> values;
		for (size_t i=0; i<sizeof(columns)/sizeof(columns[0]); i++) {
			header.addColumn(columns[i].name, columns[i].type);
			values.push_back(columns[i].value);
		}
		header.addRow(values);
		vector<unsigned char> header_data = header.write("CPK ");
		if (header_data.empty() || header_data.size() > 0x7FA) return false;
		header_data.resize(0x800, 0);
		memcpy(header_data.data() + 0x7FA, "(c)CRI", 6);
		file.seekp(0);
		return file && writeData(file, header_data.data(), header_data.size());
	}

	bool Cpk::pack(string folder, string filename, bool m_isCriLaylaCompressed) {
		filesystem::path folder_path = filesystem::u8path(folder.empty() ? "." : folder);
		filesystem::path file_path = filesystem::u8path(filename);
		vector<string> names;
		if (!readFolder(folder_path, file_path, names)) return false;

		unsigned int version;
		unsigned int revision;
		if (!CriLibXml::read(folder_path / "CriLib.xml", version, revision)) return false;
		sort(names.begin(), names.end(), [](const string &a, const string &b) {
			size_t a_slash = a.find_last_of('/');
			size_t b_slash = b.find_last_of('/');
			string a_folder = a_slash == string::npos ? "" : a.substr(0, a_slash);
			string b_folder = b_slash == string::npos ? "" : b.substr(0, b_slash);
			int order = compareNames(a_folder, b_folder);
			if (order) return order < 0;
			return compareNames(a.substr(a_slash + 1), b.substr(b_slash + 1)) < 0;
		});

		UtfTable toc("CpkTocInfo");
		toc.addColumn("DirName", 10);
		toc.addColumn("FileName", 10);
		toc.addColumn("FileSize", 4);
		toc.addColumn("ExtractSize", 4);
		toc.addColumn("FileOffset", 6);
		toc.addColumn("ID", 4);
		toc.addColumn("UserString", 10);
		struct TocRow {
			string name;
			vector<unsigned long long> values;
		};
		vector<TocRow> rows;

		ofstream file(file_path, ios::binary | ios::trunc);
		if (!file) return false;
		unsigned char padding[0x800] = {0};
		bool success = writeData(file, padding, sizeof(padding));
		unsigned long long position = 0x800;
		unsigned long long packed_size = 0;
		unsigned long long data_size = 0;
		for (size_t i=0; success && i<names.size(); i++) {
			ifstream input(folder_path / filesystem::u8path(names[i]), ios::binary | ios::ate);
			if (!input || input.tellg() < 0 || (unsigned long long)input.tellg() > 0xFFFFFFFF) {
				success = false;
				break;
			}
			unsigned int length = (unsigned int)input.tellg();
			vector<unsigned char> data(length);
			input.seekg(0);
			if (length) success = (bool)input.read((char *)data.data(), length);
			unsigned int file_size = length;
			vector<unsigned char> compressed;
			const unsigned char *buffer = data.data();
			const char csb[] = ".csb";
			// files that have the UTF header NEED to stay uncompressed, while other formats can be just fine.
			if (success && m_isCriLaylaCompressed && file_size >= 0x100 && memcmp(data.data(), "@UTF", 4) &&
				search(data.begin(), data.end(), csb, csb + 4) == data.end()) {
				success = CriLayla::compress(data.data(), file_size, compressed);
				if (success && compressed.size() < file_size) {
					buffer = compressed.data();
					file_size = (unsigned int)compressed.size();
				}
			}
			if (success) success = writeData(file, buffer, file_size);
			if (!success) break;

			size_t slash = names[i].find_last_of('/');
			TocRow row;
			row.name = names[i];
			row.values = {
				toc.addString(slash == string::npos ? "" : names[i].substr(0, slash)),
				toc.addString(names[i].substr(slash + 1)),
				file_size, length, position - 0x800, i, 0
			};
			rows.push_back(row);
			packed_size += file_size;
			data_size += length;
			position += file_size;
			size_t pad = (0x800 - position % 0x800) % 0x800;
			success = writeData(file, padding, pad);
			position += pad;
		}

		if (success) {
			// File IDs follow data order,
			// But toc rows follow full paths
			sort(rows.begin(), rows.end(), [](const TocRow &a, const TocRow &b) {
				return compareNames(a.name, b.name) < 0;
			});
			for (size_t i=0; i<rows.size(); i++) toc.addRow(rows[i].values);
			vector<unsigned char> toc_data = toc.write("TOC ");
			success = !toc_data.empty() && writeData(file, toc_data.data(), toc_data.size());
			if (success) success = writeHeader(file, position, toc_data.size(), packed_size, data_size, names.size(), version, revision);
		}
		file.close();
		if (!file) success = false;
		if (!success) {
			error_code error;
			filesystem::remove(file_path, error);
		}
		return success;
	}
};
