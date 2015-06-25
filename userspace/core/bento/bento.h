/* This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2015 Mike Gerow
 */
/* vim:tabstop=4 shiftwidth=4 noexpandtab
 */

#include <memory>

namespace bento {

const char kDefaultStorePath[] = "/var/lib/bento";

class BentoFile {
public:
	static std::unique_ptr<BentoFile> NewFromBytes(const std::string &bytes);
	static std::unique_ptr<BentoFile> NewFromPath(const std::string &path);

	const std::string &version();

private:
	const std::string version_;
};

class Store {
public:
	static std::unique_ptr<Store> New(const std::string &path);

	void AddRepo(std::unique_ptr<Repo> repo);
	void InstallFile(const BentoFile &file);
	void Install(const std::string &package_name);
	void Remove(const std::string &package_name);
};

class Repo {
public:
	static std::unique_ptr<Repo> NewFromDirectory(const std::string &path);
	static std::unique_ptr<Repo> NewFromFile(const std::string &path);
};

}  // namsepace bento
