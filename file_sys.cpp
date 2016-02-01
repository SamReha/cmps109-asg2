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
   inode root_file(file_type::DIRECTORY_TYPE, ""); // We use an empty string to identify the root directory.
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
inode::inode(file_type f_type, string inode_name): inode_nr (next_inode_nr++), name(inode_name) {
   type = f_type;

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

base_file_ptr inode::get_contents() {
   return contents;
}

directory_ptr inode::get_directory_contents() {
   if (type == file_type::PLAIN_TYPE) throw file_error ("is a plain file");
   return dynamic_pointer_cast<directory>(contents);
}

int inode::size() {
   return contents->size();
}

string inode::get_name() {
   return name;
}

void inode::set_root(inode_ptr new_root) {
   if (type == file_type::PLAIN_TYPE) throw file_error ("is a plain file");
   dynamic_pointer_cast<directory>(contents) -> setdir(string("."), new_root);
}

void inode::set_parent(inode_ptr new_parent) {
   if (type == file_type::PLAIN_TYPE) throw file_error ("is a plain file");
   dynamic_pointer_cast<directory>(contents) -> setdir(string(".."), new_parent);
}

ostream& operator<< (ostream& out, inode& node) {
   if (node.type == file_type::DIRECTORY_TYPE) {
      out << "/" << node.name << ":" << endl;
   }

   out << *dynamic_pointer_cast<directory>(node.contents);
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
size_t plain_file::size() const {
   size_t size {0};
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& plain_file::readfile() const {
   DEBUGF ('i', data);
   return data;
}

void plain_file::writefile (const wordvec& words) {
   DEBUGF ('i', words);
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
// A handy helper-function to determine who many digits wide a positive number is.
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
   for (map<string,inode_ptr>::const_iterator it = dir.dirents.begin(); it != dir.dirents.end(); ++it) {
      // Print column 1 (inode number):
      int inode_number = it->second->get_inode_nr();
      int column_one_width = 5 - get_digit_width(inode_number);

      //out << it->second << " - ";

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
}

inode_ptr directory::mkdir (const string& dirname) {
   DEBUGF ('i', dirname);
   return nullptr;
}

inode_ptr directory::mkfile (const string& filename) {
   DEBUGF ('i', filename);
   return nullptr;
}


// Updates the pointer of a given directory. If no such directory exists,
// a new directory is created with the given pointer.
void directory::setdir(string name, inode_ptr directory) {
   map<string,inode_ptr>::iterator it = dirents.find(name);
   if (it != dirents.end()) {
      dirents.erase(name);
   }

   dirents.insert(pair<string,inode_ptr>(name, directory));
}
