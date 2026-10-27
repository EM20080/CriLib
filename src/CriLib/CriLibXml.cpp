#include "CriLib.h"
#include <iterator>
#include <tinyxml2.h>

namespace CriLib {
	bool CriLibXml::read(
		const filesystem::path &filename,
		unsigned int &version,
		unsigned int &revision
	) {
		version = 7;
		revision = 14;
		ifstream file(filename, ios::binary);
		if (!file) {
			error_code error;
			if (filesystem::exists(filename, error)) return false;
			return !error;
		}

		string text((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
		if (file.bad()) return false;

		tinyxml2::XMLDocument document;
		tinyxml2::XMLError result = document.Parse(text.c_str(), text.size());
		if (result == tinyxml2::XML_ERROR_EMPTY_DOCUMENT) return true;
		if (result != tinyxml2::XML_SUCCESS) return false;

		tinyxml2::XMLElement *root = document.FirstChildElement("CriLib");
		if (root) {
			tinyxml2::XMLElement *node = root->FirstChildElement("Version");
			if (node) version = node->UnsignedText(version);
			node = root->FirstChildElement("Revision");
			if (node) revision = node->UnsignedText(revision);
		}
		return version <= 0xFFFF && revision <= 0xFFFF;
	}

	bool CriLibXml::write(
		const filesystem::path &filename,
		unsigned int version,
		unsigned int revision
	) {
		tinyxml2::XMLDocument document;
		tinyxml2::XMLElement *root = document.NewElement("CriLib");
		document.InsertEndChild(root);
		tinyxml2::XMLElement *node = document.NewElement("Version");
		node->SetText(version);
		root->InsertEndChild(node);
		node = document.NewElement("Revision");
		node->SetText(revision);
		root->InsertEndChild(node);
		tinyxml2::XMLPrinter printer;
		document.Print(&printer);

		ofstream file(filename, ios::binary | ios::trunc);
		if (!file) return false;
		file.write(printer.CStr(), printer.CStrSize() - 1);
		file.close();
		return (bool)file;
	}
};
