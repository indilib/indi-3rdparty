require 'rspec'
require 'open3'
require 'yaml'

EXIT_BAD_ARGS = 1
EXIT_OPERATION_FAILED = 2
EXIT_DEVICE_NOT_FOUND = 3
EXIT_DEVICE_MULTIPLE_FOUND = 4

def run_ticcmd(args, opts = {})
  env = {}
  cmd = 'ticcmd ' + args.to_s
  cmd = 'valgrind ' + cmd if ENV['TIC_SPEC_VALGRIND'] == 'Y'
  open3_opts = {}
  open3_opts[:stdin_data] = opts[:input] if opts.has_key?(:input)
  stdout, stderr, status = Open3.capture3(env, cmd, open3_opts)
  stdout.gsub!("\r\n", "\n")
  stderr.gsub!("\r\n", "\n")
  [stdout, stderr, status.exitstatus]
end

def tic_change_settings
  stdout, stderr, result = run_ticcmd("--get-settings -")
  if result != 0 || !stderr.empty?
    raise "Failed to get settings.  stderr=#{stderr.inspect}, result=#{result}"
  end
  settings = YAML.load(stdout)
  yield settings
  stdout, stderr, result = run_ticcmd("--set-settings -", input: YAML.dump(settings))
  if result != 0 || !stderr.empty?
    raise "Failed to set settings.  stderr=#{stderr.inspect}, result=#{result}"
  end
end

def tic_get_status
  stdout, stderr, result = run_ticcmd("-s --full")
  if result != 0 || !stderr.empty?
    raise "Failed to get status.  stderr=#{stderr.inspect}, result=#{result}"
  end
  YAML.load(stdout)
end

# Returns :T825, :T834, :T500, or :N825
def tic_product
  return @tic_product if @tic_product

  lines = %x(ticcmd --list).strip.split("\n")

  if lines.size == 0
    raise "No Tics are connected."
  end

  if lines.size > 1
    raise "Multiple Tics are connected."
  end

  md = lines.first.match(/Tic ([\w\d]+)/)
  return @tic_product = md[1].to_sym if md
end

def tic_max_allowed_current(product)
  {
    T825: 3968,
    T834: 3456,
    T500: 3093,
    N825: 3968,
    T249: 4480,
    '36v4': 9095,
  }.fetch(product)
end

TicProductSymbols = [:T825, :T834, :T500, :N825, :T249, :'36v4']
