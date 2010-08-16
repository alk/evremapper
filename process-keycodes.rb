#!/usr/bin/ruby

lines = IO.readlines("keycodes-c-input")

lines.each do |l|
  next unless l =~ /^#define\s+([A-Z_]+)\s+([0-9x]+)$/
  puts "[#{$2}] = \"#{$1}\","
end
