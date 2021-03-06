// $Id: file_sys.cpp,v 1.5 2016-01-14 16:16:52-08 - - $

#include <iostream>
#include <stdexcept>
#include <unordered_map>

using namespace std;

#include "debug.h"
#include "file_sys.h"

int inode::next_inode_nr {1};

struct file_type_hash {
   size_t operator() (file_type type) const {
      return static_cast<size_t> (type);
   }
};

ostream& operator<< (ostream& out, file_type type) {
   static unordered_map<file_type,string,file_type_hash> hash {
      {file_type::PLAIN_TYPE, "PLAIN_TYPE"},
      {file_type::DIRECTORY_TYPE, "DIRECTORY_TYPE"},
   };
   return out << hash[type];
}

/*** INODE STATE ***/
inode_state::inode_state() {
   // We use an empty string to identify the root directory.
   inode root_file(file_type::DIRECTORY_TYPE, "");
   root = make_shared<inode>(root_file);
   cwd = root;

   // Configure the parent and root dirs
   root_file.set_root(root);
   root_file.set_parent(root);

   DEBUGF ('i', "root = " << root << ", cwd = " << cwd
          << ", prompt = \"" << prompt() << "\"");
}

const string& inode_state::prompt() { return prompt_; }

inode_ptr inode_state::current_dir() {
   return cwd;
}

inode_ptr inode_state::get_root() {
   return root;
}

void inode_state::set_prompt(string new_prompt) {
   prompt_ = new_prompt;
}

void inode_state::set_directory(inode_ptr new_directory) {
   cwd = new_directory;
}

ostream& operator<< (ostream& out, const inode_state& state) {
   out << "inode_state: root = " << state.root
       << ", cwd = " << state.cwd;
   return out;
}

/*** INODE ***/
inode::inode(file_type f_type, string inode_name):
       inode_nr (next_inode_nr++) {
   type = f_type;
   name = inode_name;

   switch (type) {
      case file_type::PLAIN_TYPE:
           contents = make_shared<plain_file>();
           break;
      case file_type::DIRECTORY_TYPE:
           contents = make_shared<directory>();
           break;
   }
   DEBUGF ('i', "inode " << inode_nr << ", type = " << type);
}

int inode::get_inode_nr() const {
   DEBUGF ('i', "inode = " << inode_nr);
   return inode_nr;
}

file_type inode::get_file_type() {
   return type;
}

inode_ptr inode::get_child_directory(string name) {
   return dynamic_pointer_cast<directory>(contents) ->
                                    get_dirent(name);
}

wordvec inode::get_child_names() {
   return dynamic_pointer_cast<directory>(contents) ->
                                    get_content_labels();
}

int inode::size() {
   return contents->size();
}

string inode::get_name() {
   return name;
}

void inode::set_root(inode_ptr new_root) {
   if (type == file_type::PLAIN_TYPE) {
      throw file_error ("is a plain file");
   }

   dynamic_pointer_cast<directory>(contents) -> setdir(string("."),
                                                       new_root);
}

void inode::set_parent(inode_ptr new_parent) {
   if (type == file_type::PLAIN_TYPE) {
      throw file_error ("is a plain file");
   }

   dynamic_pointer_cast<directory>(contents) -> setdir(string(".."),
                                                       new_parent);
}

inode_ptr inode::get_parent() {
   if (type == file_type::PLAIN_TYPE) {
      throw file_error ("is a plain file");
   }

   return dynamic_pointer_cast<directory>(contents) -> get_dirent("..");
}

void inode::writefile(const wordvec& file_data) {
   if (type == file_type::DIRECTORY_TYPE) {
      throw file_error ("cannot write to directory");
   }

   dynamic_pointer_cast<plain_file>(contents) -> writefile(file_data);
}

inode_ptr inode::make_dir(string name) {
   inode_ptr new_dir = dynamic_pointer_cast<directory>(contents) ->
                                                       mkdir(name);
   new_dir -> set_parent(make_shared<inode>(*this));
   return new_dir;
}

inode_ptr inode::make_file(string name) {
   return dynamic_pointer_cast<directory>(contents) -> mkfile(name);
}

void inode::remove(string name) {
   contents -> remove(name);
}

ostream& operator<< (ostream& out, inode& node) {
   if (node.type == file_type::DIRECTORY_TYPE) {
      out << "/" << node.name << ":" << endl;
      out << *dynamic_pointer_cast<directory>(node.contents);
   } else out << *dynamic_pointer_cast<plain_file>(node.contents);

   return out;
}

/*** BASE FILE ***/
file_error::file_error (const string& what):
            runtime_error (what) {
}

ostream& operator<< (ostream& out, const base_file&) {
   out << "I am merely a basefile!";
   return out;
}

/*** PLAIN FILE ***/
ostream& operator<< (ostream& out, const plain_file& file) {
   out << file.data;
   return out;
}

plain_file::plain_file() {}

size_t plain_file::size() const {
   size_t size {data.size()}; // incomplete, needs to factor in spaces
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& plain_file::readfile() const {
   DEBUGF ('i', data);
   return data;
}

void plain_file::writefile (const wordvec& words) {
   DEBUGF ('i', words);
   data = words;
}

void plain_file::remove (const string&) {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::mkdir (const string&) {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::mkfile (const string&) {
   throw file_error ("is a plain file");
}

/*** DIRECTORY ***/
// A handy helper-function to determine who many digits wide a positive
// number is.
int get_digit_width(int number) {
   int width = 1;
   if (number >= 0) {
      while (number > 10) {
         number = number / 10;
         width++;
      }
   }

   return width;
}

ostream& operator<< (ostream& out, const directory& dir) {
   for (map<string,inode_ptr>::const_iterator it = dir.dirents.begin();
        it != dir.dirents.end();
        ++it) {
      // Print column 1 (inode number):
      int inode_number = it->second->get_inode_nr();
      int column_one_width = 5 - get_digit_width(inode_number);

      while (column_one_width > 0) {
         out << " ";
         column_one_width--;
      }
      out << inode_number;

      out << "  ";

      // Print column 2 (size):
      int size = it->second->size();
      int column_two_width = 5 - get_digit_width(size);

      while (column_two_width > 0) {
         out << " ";
         column_two_width--;
      }
      out << size;

      out << "  ";

      // Print column 3 (name):
      out << it->first;
      if (it->second->get_file_type() == file_type::DIRECTORY_TYPE
         and !(it->first == "." or it->first == "..")) {
         out << "/";
      }

      if (it != dir.dirents.end()) out << endl;
   }
   return out;
}

directory::directory() {
   dirents.insert(pair<string,inode_ptr>(".", nullptr));
   dirents.insert(pair<string,inode_ptr>("..", nullptr));
}

directory::directory(inode_ptr root, inode_ptr parent) {
   dirents.insert(pair<string,inode_ptr>(".", root));
   dirents.insert(pair<string,inode_ptr>("..", parent));

}

size_t directory::size() const {
   size_t size = dirents.size();
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& directory::readfile() const {
   throw file_error ("is a directory");
}

void directory::writefile (const wordvec&) {
   throw file_error ("is a directory");
}

void directory::remove (const string& filename) {
   DEBUGF ('i', filename);
   map<string,inode_ptr>::iterator it = dirents.find(filename);
   if (it != dirents.end()) {
      inode_ptr node_to_kill = dirents.at(filename);
      if (node_to_kill->get_file_type() == file_type::DIRECTORY_TYPE) {
         if (node_to_kill -> size() > 2) {
            throw file_error (filename +
                           "canot be removed because it is not empty");
         }

         node_to_kill -> set_root(nullptr);
         node_to_kill -> set_parent(nullptr);
      }

      dirents.erase(it);
   } else {
      throw file_error (filename +
                       " cannot be removed because it does not exist");
   }
}

inode_ptr directory::mkdir (const string& dirname) {
   DEBUGF ('i', dirname);

   map<string,inode_ptr>::iterator it = dirents.find(dirname);
   if (it != dirents.end()) {
      throw file_error (dirname + " already exists");
   }

   inode new_directory(file_type::DIRECTORY_TYPE, dirname);
   new_directory.set_root(dirents.at("."));

   inode_ptr directory_ptr = make_shared<inode>(new_directory);
   dirents.insert(pair<string,inode_ptr>(dirname, directory_ptr));

   return directory_ptr;
}

inode_ptr directory::mkfile (const string& filename) {
   DEBUGF ('i', filename);

   map<string,inode_ptr>::iterator it = dirents.find(filename);
   if (it != dirents.end()) {
      return dirents.at(filename);
   }

   inode new_file(file_type::PLAIN_TYPE, filename);

   inode_ptr file_ptr = make_shared<inode>(new_file);
   dirents.insert(pair<string,inode_ptr>(filename, file_ptr));

   return file_ptr;
}

// Updates the pointer of a given directory. If no such directory
// exists, a new directory is created with the given pointer.
void directory::setdir(string name, inode_ptr directory) {
   map<string,inode_ptr>::iterator it = dirents.find(name);
   if (it != dirents.end()) {
      dirents.erase(name);
   }

   dirents.insert(pair<string,inode_ptr>(name, directory));
}

inode_ptr directory::get_dirent(string name) {
   return dirents.at(name);
}

wordvec directory::get_content_labels() {
   wordvec labels;

   for(map<string,inode_ptr>::iterator it = dirents.begin();
       it != dirents.end();
       ++it) {
      labels.push_back(it->first);
   }

   return labels;
}
