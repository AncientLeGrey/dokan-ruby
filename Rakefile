require 'rubygems'
require 'rake'

# Source: http://dokan-dev.net/en/download/#ruby
DOKAN_RUBY_EXT = "dokan-ruby-0.1.5.1229"

begin
  require 'jeweler'
  Jeweler::Tasks.new do |gem|
    gem.name = "dokan-ruby"
    gem.platform = 'mswin32_60'
    gem.summary = %q{Ruby extension to write a windows file system (depending on Dokan library)}
    gem.description = %q{Dokan is a "file system in userspace" driver for Win32. With dokan-ruby bindings, you can implement your own filesystem in Ruby!}
    gem.email = "greyhound2@freenet.de"
    gem.homepage = "http://github.com/AncientLeGrey/dokan-ruby"
    gem.authors = ["AncientLeGrey"]
    gem.files = FileList["[A-Z]*", "{lib,test,ext,sample}/**/*"]
    gem.require_paths = ["lib", "ext/#{DOKAN_RUBY_EXT}", "ext/#{DOKAN_RUBY_EXT}/lib"]
    gem.rdoc_options = ["--exclude", "dokan_lib.c"]
    gem.add_development_dependency "shoulda"
    # gem is a Gem::Specification... see http://www.rubygems.org/read/chapter/20 for additional settings
  end
rescue LoadError
  puts "Jeweler (or a dependency) not available. Install it with: sudo gem install jeweler"
end

require 'rake/testtask'
Rake::TestTask.new(:test) do |test|
  test.libs << 'lib' << 'test'
  test.pattern = 'test/**/*_test.rb'
  test.verbose = true
end

begin
  require 'rcov/rcovtask'
  Rcov::RcovTask.new do |test|
    test.libs << 'test'
    test.pattern = 'test/**/*_test.rb'
    test.verbose = true
  end
rescue LoadError
  task :rcov do
    abort "RCov is not available. In order to run rcov, you must: sudo gem install spicycode-rcov"
  end
end

task :test => :check_dependencies

task :default => :test

require 'rake/rdoctask'
Rake::RDocTask.new do |rdoc|
  if File.exist?('VERSION')
    version = File.read('VERSION')
  else
    version = ""
  end

  rdoc.rdoc_dir = 'rdoc'
  rdoc.title = "dokan-ruby #{version}"
  rdoc.rdoc_files.include('README*')
  rdoc.rdoc_files.include('lib/**/*.rb')
end

# netbeans tasks
task :gem => :build

task :clean do
  FileUtils.rm_rf 'pkg'
end
