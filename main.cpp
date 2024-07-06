//
// librealsense2 ���g���� RealSense �̃f�v�X�摜�ƃJ���[�摜��\������
//

// RealSense
#include<librealsense2/rs.hpp>

// OpenCV
#include <opencv2/opencv.hpp>

// Windows (Visual Studio) �p�̐ݒ�
#if defined(_MSC_VER)
#  // �R���t�B�M�����[�V�����𒲂ׂ�
#  if defined(_DEBUG)
#    // �f�o�b�O�r���h�p���C�u�����������N����
#    define CONFIGURATION_STR "d.lib"
#  else
#    // Visual Studio �̃����[�X�r���h�ł̓R���\�[�����o���Ȃ�
#    pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#    // �����[�X�r���h�p���C�u�����������N����
#    define CONFIGURATION_STR ".lib"
#  endif
#  // �����N���� OpenCV �̃��C�u�����̃o�[�W����
#  define CV_VERSION_STR CVAUX_STR(CV_MAJOR_VERSION) CVAUX_STR(CV_MINOR_VERSION) CVAUX_STR(CV_SUBMINOR_VERSION)
#  // �����N���郉�C�u�������w�肷��
#  pragma comment(lib, "opencv_world" CV_VERSION_STR CONFIGURATION_STR)
#  pragma comment(lib, "realsense2.lib")
#endif

// ����͈͂̃f�v�X�̍ő�l (�P�� m)
constexpr auto MAX_DISTANCE{ 5.0 };

// �W�����C�u����
#include <iostream>

int main(int argc, char* argv[]) try
{
	// �p�C�v���C���𐶐�����
	rs2::pipeline pipe;

	// �f�t�H���g�ݒ�Ńp�C�v���C�����N�����đI�����ꂽ�f�o�C�X�������擾����
	auto selection{ pipe.start() };

	// �f�o�C�X��������f�v�X�Z���T�����o��
	auto sensor{ selection.get_device().first<rs2::depth_sensor>() };

	// �f�v�X�Z���T�̒l�̒P�ʂ����[�g���Ɋ��Z����W�����擾����
	const auto depth_scale{ sensor.get_depth_scale() };

	// �f�v�X�摜�̈ʒu���J���[�摜�ɍ��킹��t�B���^���쐬����i����x���j
	rs2::align align{ RS2_STREAM_COLOR };

	// �Z���T����擾����t���[���Z�b�g�̊i�[��
	rs2::frameset frames;

	// 1ms �҂��� ESC = 27 �������ꂽ��I������
	while (cv::waitKey(1) != 27)
	{
		// �t���[���Z�b�g���擾����
		if (pipe.poll_for_frames(&frames))
		{
			// �t���[���Z�b�g����J���[�摜�ɍ��킹���f�v�X�摜�ƃJ���[�摜�����o��
			auto aligned_frames{ align.process(frames) };

			// �t���[���Z�b�g����f�v�X�摜�ƃJ���[�摜���r�f�I�t���[���Ƃ��Ď��o��
			rs2::video_frame aligned_depth_frame{ aligned_frames.first(RS2_STREAM_DEPTH) };
			rs2::video_frame aligned_color_frame{ aligned_frames.first(RS2_STREAM_COLOR) };

			// �f�v�X�摜�ƃJ���[�摜�̕��ƍ������擾����
			const auto depth_width{ aligned_depth_frame.get_width() };
			const auto depth_height{ aligned_depth_frame.get_height() };
			const auto color_width{ aligned_color_frame.get_width() };
			const auto color_height{ aligned_color_frame.get_height() };

			// �f�v�X�摜�ƃJ���[�摜�̃f�[�^�̃|�C���^���擾����
			const auto depth_ptr{ static_cast<const uint16_t*>(aligned_depth_frame.get_data()) };
			const auto color_ptr{ static_cast<const uchar*>(aligned_color_frame.get_data()) };

			// �f�v�X�摜�ƃJ���[�摜�̃f�[�^�� OpenCV �̔z��ɕϊ�����
			const cv::Mat depth_data{ depth_height, depth_width, CV_16U, const_cast<uint16_t*>(depth_ptr) };
			const cv::Mat color_data{ color_height, color_width, CV_8UC3, const_cast<uchar*>(color_ptr) };

			// �f�v�X�摜�ƃJ���[�摜�̃f�[�^��\���ł���悤�ɕϊ�����
			cv::Mat depth, color;
			depth_data.convertTo(depth, CV_8U, 255.0 * depth_scale / MAX_DISTANCE);
			cv::cvtColor(color_data, color, cv::COLOR_RGB2BGR);

			// �摜��\������
			cv::imshow("depth", depth);
			cv::imshow("color", color);
		}
	}

	return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
	// RealSense ����������O��\������
	std::cerr
		<< "RealSense error calling "
		<< e.get_failed_function()
		<< "(" << e.get_failed_args() << "):\n    "
		<< e.what()
		<< std::endl;

	return EXIT_FAILURE;
}
catch (const std::exception& e)
{
	// ���̑��̗�O��\������
	std::cerr
		<< e.what()
		<< std::endl;

	return EXIT_FAILURE;
}
