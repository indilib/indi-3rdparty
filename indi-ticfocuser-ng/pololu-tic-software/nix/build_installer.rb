#!/usr/bin/env ruby

# This is a Ruby script that uses Git and NIX_PATH to figure out the commit
# numbers of the important repositories used to build this code.  It then sets
# the appropriate environment variables and calls nix-build to build an
# installer for the specified operating system.

require 'pathname'

if ARGV.empty?
  $stderr.puts "usage: #{$PROGRAM_NAME} ATTRPATH"
  $stderr.puts "where ATTRPATH is one of installers, win32.installer, linux-x86.installer, etc."
  exit 1
end

attr_name = ARGV.shift

# Read in the NIX_PATH as a hash.
nix_path = {}
ENV['NIX_PATH'].split(":").each do |entry|
  if md = entry.match(/(\w+)=(.+)/)
    nix_path[md[1]] = md[2]
  end
end

def get_git_commit(path)
  path = Pathname(path)
  if File.exists?(path + '.git')
    return %x(git -C #{path} rev-parse HEAD).strip
  end
  dest = path.readlink
  if md = dest.to_s.match(/\bnixpkgs-[.0-9a-z]+\.([0-9a-f]+)\b/)
    return md[1]
  end
  raise "unable to get git commit for #{path}"
end

env = {}
env['commit'] = get_git_commit('.')
env['nixcrpkgs_commit'] = get_git_commit(nix_path.fetch('nixcrpkgs'))
env['nixpkgs_commit'] = get_git_commit(nix_path.fetch('nixpkgs'))
cmd = "nix-build -A #{attr_name}"
exec(env, cmd)
