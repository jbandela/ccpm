#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <array>
#include <algorithm>
#include <exception>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>


// See http://stackoverflow.com/questions/2505042/how-to-parse-a-tar-file-in-c for details

// Each header is 512 bytes
typedef std::array< char, 512> header_t;

enum class entry_type{
	file, directory
};

namespace detail{

	std::string get_magic(const header_t& h){
		auto start = h.data() + 257;
		 std::string ret { start, std::find(start,start + 6, 0) };
		 boost::algorithm::trim(ret);
		 return ret;
	}

	std::string get_filename(const header_t& h){
		std::string ret{ h.data(), std::find(h.data(),h.data() + 100,0) };
		if (get_magic(h) == "ustar"){
			auto start = h.data() + 345;
			std::string prefix{ start, std::find(start, start + 155, 0) };
			if (prefix.size()){
				ret = prefix + "/" + ret;
			}
		}
		return ret;
	}
	entry_type get_type(const header_t& h){
		auto t = h[156];
		if (t == 0 || t == '0') return entry_type::file;
		if (t == '5') return entry_type::directory;
		// Only support files and directories
		throw std::runtime_error("Invalid entry type");

	}

	// From above stack overflow
	int octal_string_to_int(const  char *current_char, unsigned int size){
		std::size_t output = 0;
		while (size > 0){
			output = output * 8 + *current_char - '0';
			current_char++;
			size--;
		}
		return output;
	}

	std::size_t get_filesize(const header_t& h) { 
		
		
		return  octal_string_to_int(&h[124], 11); }

	// elsewhere

	

}

struct tar_archive{
	std::istream& fs_;
	header_t h_;
	bool fdata_read_ = false;
	std::vector<char> vec_;
	boost::filesystem::path base_path_;
	void read_header(){
		fs_.read(h_.data(), h_.size());
		fdata_read_ = false;
		std::string magic;
		if ((magic = detail::get_magic(h_)) != "ustar" && magic.size()){
			throw std::runtime_error("Only the ustar format is supported, unsupported format");
		}
	}
	tar_archive(std::istream& fs) :fs_{ fs}{
		
		read_header();
	}

	boost::filesystem::path path(){
		return detail::get_filename(h_);
	}

	entry_type type(){
		return detail::get_type(h_);
	}

	std::size_t file_size(){
		return detail::get_filesize(h_);
	}
	
	const std::vector<char>& file_data(){
		if (fdata_read_) return vec_;
		vec_.clear();
		fdata_read_ = true;
		auto sz = file_size();
		header_t buf;
		std::size_t bytes_read = 0;
		while (sz){
			fs_.read(buf.data(), buf.size());
			vec_.insert(vec_.end(), buf.data(), buf.data() + std::min(sz, buf.size()));
			sz -= std::min(buf.size(), sz);
			bytes_read += 512;
		}

		return vec_;
	}

	void extract(const boost::filesystem::path& p){
		if (type() == entry_type::directory){
			boost::filesystem::create_directories(p / path());
		}
		else{
			boost::filesystem::create_directories(p / path().parent_path());
			boost::filesystem::ofstream ofs{ p / path() };
			if (file_data().size()){
				ofs.write(&file_data()[0], file_data().size());
			}
			ofs.close();

		}
	}
	bool next(){
		if (!fdata_read_)file_data();
		read_header();
		if (detail::get_filename(h_).empty()){
			return false;
		}
		else{
			return true;
		}
	}


};

void extract_all(const boost::filesystem::path& archive_path, const boost::filesystem::path& p){

	boost::filesystem::ifstream ifs;
	ifs.exceptions(std::ios::failbit);
	ifs.open( archive_path,std::ios::binary );
	boost::iostreams::filtering_istream fifs;
	auto ext = boost::algorithm::to_lower_copy(archive_path.extension().string());


	if (ext == ".gz" || ext == ".tgz"){
		fifs.push(boost::iostreams::gzip_decompressor{});
	}
	fifs.push(ifs);
	tar_archive ar{ fifs };
	do{
		ar.extract(p);
	} while (ar.next());
}

int main(int argc, char** argv){

	try{
	auto cp = boost::filesystem::current_path();

	if (argc < 2){
		std::cout << "Enter tar file\n";
		return 0;
	}
	extract_all(argv[1], cp);
	}
	catch (std::exception& e){
		std::cerr << "Error " << e.what();
	}

}