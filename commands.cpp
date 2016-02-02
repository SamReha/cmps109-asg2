// $Id: commands.cpp,v 1.16 2016-01-14 16:10:40-08 - - $

#include "commands.h"
#include "debug.h"
#include <regex>

command_hash cmd_hash {
   {"cat"   , fn_cat   },
   {"cd"    , fn_cd    },
   {"echo"  , fn_echo  },
   {"exit"  , fn_exit  },
   {"ls"    , fn_ls    },
   {"lsr"   , fn_lsr   },
   {"make"  , fn_make  },
   {"mkdir" , fn_mkdir },
   {"prompt", fn_prompt},
   {"pwd"   , fn_pwd   },
   {"rm"    , fn_rm    },
};

command_fn find_command_fn (const string& cmd) {
   // Note: value_type is pair<const key_type, mapped_type>
   // So: iterator->first is key_type (string)
   // So: iterator->second is mapped_type (command_fn)
   const auto result = cmd_hash.find (cmd);
   if (result == cmd_hash.end()) {
      throw command_error (cmd + ": no such function");
   }
   return result->second;
}

command_error::command_error (const string& what):
            runtime_error (what) {
}

int exit_status_message() {
   int exit_status = exit_status::get();
   cout << execname() << ": exit(" << exit_status << ")" << endl;
   return exit_status;
}

/* check_validity -
      Checks if a given wordvec represents a valid path either from root or
      from the current directory as specified by the third parameter. Doesn't
      live in util.cpp because it needs to know what states are.
*/
inode_ptr check_validity(inode_state& state, wordvec path_to_check, bool check_from_root) {
   inode_ptr position = (check_from_root) ? state.get_root() : state.current_dir();
   int depth = 0;

   while (depth < path_to_check.size()) {
      try {
         position = position->get_child_directory(path_to_check.at(depth));
         depth++;
      } catch (...) {
         throw command_error ("file system: path does not exist");
      }
   }

   return position;
}

void fn_cat (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // First, let's check our arguments - we should have one or more
   if (words.size() < 2) throw command_error ("cat: too few operands");

   // Print the contents of each file to cout
   for (uint i = 1; i < words.size(); i++) {
      // First, let's try and parse the file path string into a wordvec
      wordvec file_path = split(words.at(i), "/");
      inode destination = *check_validity(state, file_path, (words.at(1).at(0) == '/'));

      // Check if the file is a file, and then print it oot.
      if (destination.get_file_type() == file_type::PLAIN_TYPE) {
         cout << destination << endl;
         //cout << "TEST!" << endl;
      } else throw command_error ("cat: can't cat a directory!");
   }
}

void fn_cd (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // First, let's check our arguments. We shouldn't have more than one.
   if (words.size() > 2) throw command_error ("ls: too many operands");

   // If we're given an argument, see if it's a valid path and change to that
   // directory
   if (words.size() == 2) {
      // First, let's try and parse the file path string into a wordvec
      wordvec file_path = split(words.at(1), "/");
      inode_ptr destination_dir = check_validity(state, file_path, (words.at(1).at(0) == '/'));

      // Change current dir
      state.set_directory(destination_dir);
   }

   // else, if no arguments given, return to root
   else {
      state.set_directory(state.get_root());
   }
}

void fn_echo (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);
   cout << word_range (words.cbegin() + 1, words.cend()) << endl;
}

void fn_exit (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // Decode our exit status
   int status = 0;
   if (words.size() > 1) {
      // We only care about the value of the first token after the command itself
      string exitArg = words.at(1);
      for (uint i = 0; i < exitArg.size(); i++) {
         // if (exitArt is non-numeric) {
         if (exitArg.at(i) < '0' or exitArg.at(i) > '9') {
            status = 127;
            break;
         }
      }
      if (status != 127) status = stoi(exitArg);
   }
   exit_status::set(status);

   throw ysh_exit();
}

void fn_ls (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // If we're given an argument, see if it's a valid path and show that
   if (words.size() >= 2) {
      for (uint i = 1; i < words.size(); i++) {
         // First, let's try and parse the file path string into a wordvec
         wordvec file_path = split(words.at(i), "/");
         inode destination_dir = *check_validity(state, file_path, (words.at(i).at(0) == '/'));

         // Show the file
         cout << destination_dir << endl;
      }
   }
   // Otherwise, show the contents of the current location
   else {
      inode currentDir = *state.current_dir();
      cout << currentDir << endl;
   }
}

void recursive_print(inode_ptr inode) {
   cout << *inode << endl;
   wordvec child_names = inode -> get_child_names();

   // Start at two so we skip . and ..
   for (uint i = 2; i < child_names.size(); i++) {
      inode_ptr child = inode -> get_child_directory(child_names.at(i));

      if (child -> get_file_type() == file_type::DIRECTORY_TYPE) {
         recursive_print(child);
      }
   }
}

void fn_lsr (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // If we're given an argument, see if it's a valid path and show that
   if (words.size() >= 2) {
      for (uint i = 1; i < words.size(); i++) {
         // First, let's try and parse the file path string into a wordvec
         wordvec file_path = split(words.at(i), "/");
         inode_ptr destination_dir = check_validity(state, file_path, (words.at(i).at(0) == '/'));

         // Show the file
         recursive_print(destination_dir);
      }
   }
   // Otherwise, show the contents of the current location
   else {
      inode_ptr currentDir = state.current_dir();
      recursive_print(currentDir);
   }
}

/* make_helper
      fn_make and fn_mkdir have a lot of shared logic, but are still called as
      separate functions. make_helper handles logic that is common to both
      functions.
*/
inode_ptr make_helper(inode_state& state, const wordvec& words, bool is_directory) {
   // First, let's try and parse the file path string into a wordvec
   wordvec file_path = split(words.at(1), "/");

   // Then, we'll check to see if the path is valid
   wordvec path_to_check = file_path;

   // We don't bother to check the last element, because that will be the new
   // element
   bool make_from_root = (words.at(1).at(0) == '/');
   path_to_check.erase(path_to_check.end());

   inode_ptr destination_dir = check_validity(state, path_to_check, make_from_root);

   // Create the new file
   if (is_directory) {
      return destination_dir->make_dir(file_path.back());
   } else return destination_dir->make_file(file_path.back());
}

void fn_make (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // First, let's check our arguments. We should have at least one.
   if (words.size() == 1) throw command_error ("make: missing operands");

   inode_ptr new_file = make_helper(state, words, false);

   // Trim out of the first two elements of words (the function name and
   // location) and write the remainder to the new file.
   wordvec file_contents = words;
   file_contents.erase(file_contents.begin());
   file_contents.erase(file_contents.begin());
   new_file -> writefile(file_contents);
}

void fn_mkdir (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   // First, let's check our arguments. We should have exactly one.
   if (words.size() == 1) {
      throw command_error ("mkdir: missing operand");
   } else if (words.size() > 2) {
      throw command_error ("mkdir: only one operand allowed");
   }

   make_helper(state, words, true);
}

void fn_prompt (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   if (words.size() > 1) {
      string new_prompt("");
      for (uint i = 1; i < words.size(); i++) {
         new_prompt = new_prompt + words.at(i) + " ";
      }

      state.set_prompt(new_prompt);
   }
}

void fn_pwd (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);

   inode_ptr location = state.current_dir();
   wordvec current_path;

   while (location != state.get_root()) {
      current_path.insert(current_path.begin(), location -> get_name());
      location = location -> get_parent();
   }

   for (uint i = 1; i < current_path.size(); i++) {
      cout << "/" << current_path.at(i);
   }

   if (current_path.size() == 1) cout << "/";
   cout << endl;
}

void fn_rm (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);
}

void fn_rmr (inode_state& state, const wordvec& words){
   DEBUGF ('c', state);
   DEBUGF ('c', words);
}
