#include "CriLib.h"

int main(int argc, char **argv) {
	if (argc < 2) return 1;
	error_code error;
	if (filesystem::is_directory(filesystem::u8path(argv[1]), error)) {
		string folder = argv[1];
		while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\')) folder.pop_back();
		bool m_isCriLaylaCompressed = true;
		return CriLib::Cpk::pack(argv[1], argc > 2 ? argv[2] : folder + ".cpk", m_isCriLaylaCompressed) ? 0 : 1;
	}
	string name = argv[1];
	name = name.substr(name.find_last_of("/\\") + 1);
	name = name.substr(0, name.find_last_of('.'));
	CriLib::Cpk cpk(argv[1]);
	return cpk.extract(argc > 2 ? argv[2] : name) ? 0 : 1;
}
