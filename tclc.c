/*
 * This file is licensed under the MIT/X Consortium License.
 *
 * © 2010 Nicolai Waniek
 *
 * See LICENSE file for further copyright and license details.
 * See README file for a detailed description of tclc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <CL/cl.h>


typedef enum
{
	ARG_H = 0,
	ARG_V,
	ARG_L,
	ARG_D,

	ARG_UNK
} arg_t;


typedef enum
{
	DT_GPU = 0,
	DT_CPU
} dt_t;


typedef struct src src_t;
struct src
{
	char *fname;
	struct stat sb;
	src_t *next;
};


typedef struct env
{
	dt_t dt;
	src_t *src;
} env_t;


void list ();
void compile (src_t *src, cl_context context, cl_device_id *devs, cl_uint ndevs);
void process (env_t *env);


const char*
usage ()
{
	return "Usage: tclc [options] filename...\n"
               "Options:\n"
               "  -d <arg>     Specify the device type. arg is either CPU or GPU. default is GPU\n"
	       "  -l           List all available platforms, devices and device types\n"
	       "  -v           Print version information\n"
	       "  -h, --help   Show this help\n"
	       ;
}


void
die (const char *errmsg, ...)
{
	va_list ap;
	va_start(ap, errmsg);
	vfprintf(stderr, errmsg, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


arg_t
get_arg_t (const char *arg)
{
	if (!strncmp(arg, "-h", 2) || !strncmp(arg, "--help", 6)) {
		fprintf(stdout, usage());
		exit(EXIT_SUCCESS);
	}

	if (!strncmp(arg, "-v", 2)) {
		fprintf(stdout, "tclc "VERSION" © 2010 Nicolai Waniek, see LICENSE for details\n");
		exit(EXIT_SUCCESS);
	}

	if (!strncmp(arg, "-d", 2))
		return ARG_D;
	if (!strncmp(arg, "-l", 2))
		return ARG_L;

	return ARG_UNK;
}


void
parse_args (int argc, const char **argv, env_t* env)
{
	if (argc < 2)
		return;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		src_t *src;
		int opt_ok = 0;

		switch (get_arg_t(arg)) {
		case ARG_D:
			i++;
			opt_ok = 0;

			if (i >= argc)
				die("ERROR: Insufficient argument -d\n");

			if (!strncmp(argv[i], "CPU", 3)) {
				env->dt = DT_CPU;
				opt_ok = 1;
			}

			if (!strncmp(argv[i], "GPU", 3)) {
				env->dt = DT_GPU;
				opt_ok = 1;
			}

			if (!opt_ok)
				die("ERROR: Invalid Platform %s\n", argv[i]);

			break;

		case ARG_L:
			list();
			exit(EXIT_SUCCESS);
			break;

		default:
			src = (src_t*)malloc(sizeof(src_t));
			src->next = NULL;
			src->fname = (char*)malloc(sizeof(char) * strlen(arg));
			strcpy(src->fname, arg);

			if (stat(src->fname, &src->sb) < 0)
				die("ERROR: Could not open file %s\n", arg);

			if (S_ISDIR(src->sb.st_mode))
				die("ERROR: Cannot handle directories\n");

			if (!env->src) {
				env->src = src;
			}
			else {
				src_t  *s = env->src;
				while (s->next)
					s = s->next;
				s->next = src;
			}
		}
	}
}


void
clean (env_t *env)
{
	src_t *src = env->src;
	while (src) {
		src_t *s = src;
		src = src->next;
		free(s->fname);
		free(s);
	}
	env->src = NULL;
}


int
main (int argc, const char **argv)
{
	if (argc < 2)
		die(usage());

	env_t env = {0, NULL};
	parse_args(argc, argv, &env);
	if (env.src)
		process(&env);
	clean(&env);

	return EXIT_SUCCESS;
}


/*
 * OpenCL invoking functions
 */


void
print_platform_info (cl_platform_id pid)
{
	size_t len;
	char *name;

	if (clGetPlatformInfo(pid, CL_PLATFORM_NAME, 0, NULL, &len) != CL_SUCCESS)
		die("Error: Could not get platform name length\n");

	name = (char*)malloc(sizeof(char) * len);
	if (clGetPlatformInfo(pid, CL_PLATFORM_NAME, len, name, NULL) != CL_SUCCESS)
		die("Error: Could not get platform name\n");

	printf("Platform: %s\n", name);
	free(name);
}


void
print_device_info (cl_uint i, cl_device_id did)
{
	size_t len;
	char *name;
	cl_device_type type;
	const char *type_repr;

	if (clGetDeviceInfo(did, CL_DEVICE_NAME, 0, NULL, &len) != CL_SUCCESS)
		die("ERROR: Could not get device name length\n");

	name = (char*)malloc(sizeof(char) * len);
	if (clGetDeviceInfo(did, CL_DEVICE_NAME, len, name, 0) != CL_SUCCESS)
		die("ERROR: Could not get device name\n");

	if (clGetDeviceInfo(did, CL_DEVICE_TYPE, sizeof(cl_device_type), &type,
				NULL) != CL_SUCCESS)
		die("Error: Could not determine device type\n");

	switch (type) {
	case CL_DEVICE_TYPE_CPU:
		type_repr = "CPU";
		break;

	case CL_DEVICE_TYPE_GPU:
		type_repr = "GPU";
		break;

	case CL_DEVICE_TYPE_ACCELERATOR:
		type_repr = "ACCELERATOR";
		break;

	case CL_DEVICE_TYPE_DEFAULT:
		type_repr = "DEFAULT";
		break;

	default:
		type_repr = "Unknown";
	}

	printf("    Device %u: %s\n"
	       "        Type: %s\n", i, name, type_repr);
	free(name);
}


cl_device_id*
get_device_ids (cl_platform_id pid, cl_uint *idcount)
{
	cl_uint ndevs;
	if (clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, 0, NULL, &ndevs) != CL_SUCCESS)
		die("ERROR: Could not determine number of devices\n");
	if (idcount)
		*idcount = ndevs;

	cl_device_id *devs = (cl_device_id*)malloc(sizeof(cl_device_id) * ndevs);
	if (clGetDeviceIDs(pid, CL_DEVICE_TYPE_ALL, ndevs, devs, 0) != CL_SUCCESS)
		die("ERROR: Could not determine device IDs\n");

	return devs;
}


void
list_devices (cl_platform_id pid)
{
	cl_uint ndevs;
	cl_device_id *devs = get_device_ids(pid, &ndevs);

	for (cl_uint i = 0; i < ndevs; i++)
		print_device_info(i, devs[i]);

	free(devs);
}


void
list ()
{
	cl_uint nplatforms;
	if (clGetPlatformIDs(0, NULL, &nplatforms) != CL_SUCCESS)
		die("ERROR: Could not determine number of platforms\n");

	cl_platform_id *platforms = (cl_platform_id*)malloc(
			sizeof(cl_platform_id) * nplatforms);
	if (clGetPlatformIDs(nplatforms, platforms, NULL) != CL_SUCCESS)
		die("ERROR: Could not determine platform IDs\n");

	for (cl_uint i = 0; i < nplatforms; i++) {
		print_platform_info(platforms[i]);
		list_devices(platforms[i]);
	}

	free(platforms);
}


void
handle_compile_error (cl_int err, cl_program prog, cl_device_id *devs,
		cl_uint ndevs)
{
	if (err == CL_SUCCESS)
		return;

	// currently just handle build program failures properly
	if (err != CL_BUILD_PROGRAM_FAILURE)
		die("ERROR: Unspecified build failure\n");

	cl_build_status status;
	char *log = NULL;
	size_t len = 0;

	for (cl_uint i = 0; i < ndevs; i++) {
		err = clGetProgramBuildInfo(prog, devs[i],
				CL_PROGRAM_BUILD_STATUS,
				sizeof(cl_build_status), &status, NULL);

		if (err != CL_SUCCESS)
			continue;

		if (status != CL_BUILD_ERROR)
			continue;

		err = clGetProgramBuildInfo(prog, devs[i],
				CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
		if (err != CL_SUCCESS)
			die("ERROR: Could not retrieve build log size\n");

		log = (char*)malloc(sizeof(char) * len);
		memset(log, 0, len);
		err = clGetProgramBuildInfo(prog, devs[i],
				CL_PROGRAM_BUILD_LOG, len, log, NULL);
		if (err != CL_SUCCESS)
			die("ERROR: Could not retrieve build log\n");

		printf("%s\n", log);
		free(log);
	}
}


void
compile (src_t *src, cl_context context, cl_device_id *devs, cl_uint ndevs)
{
	const char *source;
	int fd;
	cl_int err;

	if ((fd = open(src->fname, O_RDONLY)) == -1)
		die("ERROR: Could not open file %s\n", src->fname);
	source = mmap(NULL, src->sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	size_t len[] = {strlen(source)};

	cl_program prog = clCreateProgramWithSource(context, 1, &source,
			len, &err);
	if (err != CL_SUCCESS)
		die("ERROR: Could not create program from source\n");

	err = clBuildProgram(prog, 0, NULL, NULL, NULL, NULL);
	handle_compile_error(err, prog, devs, ndevs);

	clReleaseProgram(prog);
	munmap((void*)source, src->sb.st_size);
	close(fd);
}


void
handle_context_error (cl_uint err)
{
	if (err == CL_SUCCESS)
		return;

	/* at the moment output the most likely errors. for unprobable ones just
	 * use a default error message */
	switch (err) {
	case CL_INVALID_DEVICE_TYPE:
		die("ERROR: Chosen device type is not available on your system.\n");

	case CL_INVALID_PLATFORM:
		die("ERROR: Selected platform is invalid\n");

	default:
		die("ERROR: Could not create context\n");
	}
}


cl_context
create_context (env_t *env, cl_platform_id *platform)
{
	cl_int err;
	cl_context context;
	cl_platform_id pid = NULL;
	cl_device_type type = (env->dt == DT_CPU)
		? CL_DEVICE_TYPE_CPU
		: CL_DEVICE_TYPE_GPU;

	if (clGetPlatformIDs(1, &pid, NULL) != CL_SUCCESS)
		die("ERROR: Could not select platform\n");

	cl_context_properties props[3] = {CL_CONTEXT_PLATFORM,
		(cl_context_properties)pid, 0};

	context = clCreateContextFromType(props, type, NULL, NULL, &err);
	handle_context_error(err);

	if (platform)
		*platform = pid;

	return context;
}


void
process (env_t *env)
{
	cl_platform_id pid;
	cl_context context = create_context(env, &pid);

	cl_uint ndevs;
	cl_device_id *devs = get_device_ids(pid, &ndevs);

	src_t* src = env->src;
	while (src) {
		compile(src, context, devs, ndevs);
		src = src->next;
	}

	free(devs);
	clReleaseContext(context);
}
