#!/usr/bin/ruby

names = Dir['/sys/class/input/*/name']

names = names.map do |n|
  [n, IO.read(n).strip]
end

keyboard_name, = names.detect do |n,v|
  v =~ /^AT.*\skeyboard/
end

raise "couldn't find keyboard input device" unless keyboard_name

video_bus, = names.detect do |n,v|
  v == 'Video Bus'
end

def path_to_dev(path)
  path =~ %r|/input(\d+)$|
  "/dev/input/event#{$1}"
end

argv = [File.join(File.dirname($0), 'repeater'), *ARGV]

argv << path_to_dev(File.dirname(keyboard_name))

if video_bus
  argv << path_to_dev(File.dirname(video_bus))
end

exec(*argv)
