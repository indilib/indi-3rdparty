def compute_ilim(dac_level)
  v_iset = 0.9
  v_dac_top = 4.096
  r_top = 107000.0
  r_bot = 33000.0

  r_dac_top = (32 - dac_level) * 5000
  r_dac_bot = dac_level * 5000

  v_dacout = v_dac_top * r_dac_bot / (r_dac_bot + r_dac_top)
  if dac_level == 0
    r_dacout = 0
  else
    r_dacout = 1 / (1 / r_dac_top.to_f + 1 / r_dac_bot.to_f)
  end

  iset_current_sourced =
    (v_iset - v_dacout) / (r_top + r_dacout) + v_iset / r_bot

  if iset_current_sourced < 0
    iset_current_sourced = 0
  end

  iset_current_sourced * 86666
end

puts "  0,"
31.downto(0) do |dac_level|
  puts "  %d," % [(compute_ilim(dac_level) * 1000).round]
end

