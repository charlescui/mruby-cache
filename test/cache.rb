def setup
	$cache_x = Cache.new 'filename'=>"./foo.lmc", 'min_alloc_size' => 512
	$cache_y = Cache.new 'filename'=>"./foo.lmc", 'min_alloc_size' => 512
end

$assertions = {
	:success => 0,
	:failed => 0
}

def assert(title='')
	$stderr.puts "***#{title}"
	if block_given? and yield
		$stderr.puts "--Success"
		$assertions[:success] += 1
	else
		$stderr.puts "--Failed"
		$assertions[:failed] += 1
	end
end

setup

assert('set value') do
	($cache_x['test']='hello') == 'hello'
end

assert('get value') do
	$cache_x['test'] == $cache_y['test']
end

assert('shm_status keys') do
	status = $cache_x.shm_status
	status.keys.sort == [:free_bytes, :free_chunks, :largest_chunk, :total_bytes, :used_bytes]
end

assert('delete key') do
	($cache_x.delete('test') == true) and ($cache_y.delete('test') == false)
end

assert('fetch deleted key') do
	($cache_x['test'] == nil) and ($cache_y['test'] == nil)
end

total = $assertions[:success]+$assertions[:failed]
$stderr.puts "\nTotal assertions:#{total}, success:#{$assertions[:success]}, failed:#{$assertions[:failed]}"




