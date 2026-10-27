#pragma once

namespace CriLib {
	class CriLibXml {
	public:
		static bool read(
			const filesystem::path &filename,
			unsigned int &version,
			unsigned int &revision
		);

		static bool write(
			const filesystem::path &filename,
			unsigned int version,
			unsigned int revision
		);
	};
};
