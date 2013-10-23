#!/usr/bin/env ruby

def eachApp
    File.open("apps.stat", "r").grep /^[^#]/ do |line|
        yield line.chomp.split(',')
    end
end

apps = Array.new
eachApp { |values| apps << [ values[6].to_i, values[6].to_i + values[9].to_i, values[11].to_f ] }

0.step(216000, 1000) do |time|
	max = 0.0
	apps.each do |release, endtime, slowness|
        max = slowness if release <= time and endtime > time and max < slowness
    end
	puts "#{time},#{max}"
end

