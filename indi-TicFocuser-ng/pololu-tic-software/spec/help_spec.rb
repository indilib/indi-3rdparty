require 'spec_helper'

describe 'help screen' do
  it 'fits in 80 columns' do
    help, stderr, result = run_ticcmd("-h")
    expect(stderr).to eq ''
    expect(result).to eq 0
    help.each_line do |line|
      line = line.chomp
      if line.size > 80
        raise "help line too long: #{line.inspect}"
      end
    end
  end
end
